
#include <cassert>
#include <chrono>

#include <deque>
#include <map>
#include <vector>

#include <stdexcept>
#include <thread>

#include <cmath>
#include <iomanip>
#include <ostream>


// basic event
struct event {
  using clock_type = std::chrono::high_resolution_clock;
  using time_type = clock_type::time_point;
  using id_type = const char*;

  id_type id;
  time_type time;

  enum { BEGIN, END } kind;

  // named constructors
  static inline event begin(id_type id) { 
    return {id, clock_type::now(), BEGIN};
  }

  static inline event end(id_type id) {
    return {id, clock_type::now(), END};
  }
};


// thread-local event sequence
class timeline {
  std::thread::id thread;

  using storage_type = std::deque<event>;
  storage_type storage;

  using instances_type = std::deque<timeline*>;
  static instances_type instances;

  timeline(): thread(std::this_thread::get_id()) {
    instances.emplace_back(this);
  }

  static timeline& current() {
    // note: magic statics protect writing to `instances`
    static thread_local timeline instance;
    return instance;
  }

public:
  static void push(event ev) { current().storage.push_back(ev); }

  void clear() { current().storage.clear(); }

  auto begin() const { return storage.begin(); }
  auto end() const { return storage.end(); }

  static const auto& all() { return instances; }
};

timeline::instances_type timeline::instances;


// scope-guard for recording events
class timer {
  event::id_type id;
  timer(const timer&) = delete;

  public:
  timer(event::id_type id): id(id) { timeline::push(event::begin(id)); }

  ~timer() { timeline::push(event::end(id)); }
};


// general tree structure
template<class Derived>
struct tree {
  std::vector<Derived> children;

  // TODO catamorphisms
};

struct call: tree<call> {
  event::id_type id;

  // total duration is the sum of this (used to show stats)
  using duration_type = std::chrono::microseconds;
  std::vector<duration_type> duration;
  
  call() = default;
  
  call(const timeline& events) {
    auto it = events.begin();
    auto end = events.end();
    if(!parse(*this, it, end)) {
      throw std::runtime_error("parse error");
    }
  }


  // TODO optimize when lhs is moved-from?
  // TODO
  friend call merge(const call& lhs, const call& rhs) {
    if(lhs.id != rhs.id) {
      throw std::logic_error("cannot merge unrelated call trees");
    }

    call result;
    result.id = lhs.id;

    // concatenate durations
    std::copy(lhs.duration.begin(),
              lhs.duration.end(),
              std::back_inserter(result.duration));
    std::copy(rhs.duration.begin(),
              rhs.duration.end(),
              std::back_inserter(result.duration));

    // note: callees may differ due to different runtime conditions
    std::map<event::id_type, std::vector<const call*>> sources;
    for(auto& callee: lhs.children) {
      sources[callee.id].emplace_back(&callee);
    }

    for(auto& callee: rhs.children) {
      sources[callee.id].emplace_back(&callee);
    }

    // TODO preserve ordering as much as possible?
    // TODO or at least make merge associative (or is it already since we order
    // by id?)
    for(auto& source: sources) {
      switch(source.second.size()) {
      case 2:
        result.children.emplace_back(
            merge(*source.second.front(), *source.second.back()));
        break;
      case 1:
        result.children.emplace_back(*source.second.front());
        break;
      default:
        assert(false);
      }
    }

    return result;
  }


  call simplify() const {
    std::map<event::id_type, std::vector<call>> simplified;
    for(auto& it: children) {
      simplified[it.id].emplace_back(it.simplify());
    }

    call result;
    result.id = id;
    result.duration = duration;
      
    for(auto& it: simplified) {
      call merged;
      merged.id = it.first;
      
      for(auto& callee: it.second) {
        merged = merge(merged, callee);
      }

      result.children.emplace_back(std::move(merged));
    }

    return result;
  }

private:
  template<class Iterator>
  static bool parse(call& result, Iterator& first, Iterator last) {
    switch(first->kind) {
    case event::END:
      return false;
    case event::BEGIN: {
      result.id = first->id;
      const event::time_type begin = first->time;
      ++first;

      // try to parse callees
      for(call child; parse(child, first, last);
          result.children.emplace_back(std::move(child))) {
      }

      // try to finish this parse
      assert(first->kind == event::END);
      if(first->id == result.id) {
        result.duration.clear();
        result.duration.emplace_back(
            std::chrono::duration_cast<duration_type>(first->time - begin));

        ++first;
        return true;
      }

      return false;
    }

    default:
      throw std::logic_error("invalid event kind");
    };
    }
};


struct report: tree<report> {
  double total = 0;
  double mean = 0;
  double dev = 0;
  double percent = 0;
  std::size_t count = 0;
  const char* id;

  report(const call& self, double caller_total=0) {
    call::duration_type sum(0);
    assert(!self.duration.empty());
    
    for(auto& duration: self.duration) {
      sum += duration;
      ++count;
    }

    // note: milliseconds
    const double ms = 1000;
    total = sum.count() / ms;
    percent = caller_total ? (total / caller_total) * 100 : 100;
    mean = count ? total / count : 0;
    id = self.id;

    for(auto& duration: self.duration) {
      const double delta = (duration.count() / ms - mean);
      dev += delta * delta;
    }

    dev = count > 1 ? std::sqrt(dev / (count - 1)) : 0;


    for(auto& callee: self.children) {
      children.emplace_back(callee, total);
    }
  }


  template<class Count, class Total, class Mean, class Dev, class Percent, class Id>
  static void write_row(std::ostream& out,
                        Count count,
                        Total total,
                        Mean mean,
                        Dev dev,
                        Percent percent,
                        Id id,
                        std::size_t depth = 0) {
    const std::size_t width = 14;
    out << std::left << std::fixed << std::setprecision(2)
        << std::right << std::setw(width / 2) << count
        << std::right << std::setw(width) << total
        << std::right << std::setw(width) << mean
        << std::right << std::setw(width) << dev      
        << std::right << std::setw(width) << percent
        << std::left << std::setw(width) << " " + std::string(depth, '.') + id
        << '\n';

  }

  static void write_header(std::ostream& out) {
    return write_row(out, "count", "total", "mean", "dev", "%", "id");
  }

  void write(std::ostream& out, std::size_t depth = 0) const {
    write_row(out, count, total, mean, dev, percent, id, depth);
    for(auto& it: children) {
      it.write(out, depth + 1);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////

#include <iostream>

void work() {
  const timer foo("foo");
  for(std::size_t i = 0; i < 2; ++i) {
    const timer bar("bar");
    for(std::size_t j = 0; j < 10; ++j) {
      {
        const timer bar("baz");
        std::clog << i << " " << j << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      
      {
        const timer quxx("quxx");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
  }
}


int main(int, char**) {
  work();

  for(auto& events: timeline::all()) {
    report::write_header(std::cout);
    report(call(*events).simplify()).write(std::cout);

    events->clear();
  }

  return 0;
}
