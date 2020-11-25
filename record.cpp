
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

  std::size_t timestamp() const {
    return (time - origin).count();
  }
  
  static const time_type origin;
};

const event::time_type event::origin = event::clock_type::now();

// thread-local event sequence
class timeline {
public:
  const std::thread::id thread;
private:
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
};


// call tree
struct call: tree<call> {
  event::id_type id;

  // total duration is the sum of these (used to produce stats)
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
  // TODO check this is associative
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

  // merge all callees with same id into a single one
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


// call tree reporing
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


struct flame {
  using stack_type = std::deque<event::id_type>;
  stack_type stack;
  
  std::size_t count;
};


#include <iostream>

call::duration_type flame_graph(std::deque<flame>& flames, const call& self) {
  // push ourselves to the stack
  flames.back().stack.emplace_back(self.id);

  // save stack
  const auto stack = flames.back().stack;
  
  call::duration_type children_duration(0);
  for(const auto& child: self.children) {
    children_duration += flame_graph(flames, child);

    // add a new frame next child/ourselves
    flames.emplace_back();
    flames.back().stack = stack;
  }

  // make sure we did not screw the stack
  // assert(flames.back().stack.back() == self.id);

  // add remaining time
  call::duration_type current_duration(0);
  for(auto duration: self.duration) {
    current_duration += duration;
  }

  const call::duration_type remaining = current_duration - children_duration;
  flames.back().count = remaining.count();

  // std::clog << self.id << " remaining: " << remaining.count() << std::endl;
  
  // this stack frame is finished, pop ourselves
  // flames.back().stack = stack;
  // flames.back().stack.pop_back();

  return current_duration;
}



// flame graph
void flame_graph(std::ostream& out, const call& self) {
  std::deque<flame> flames;
  flames.emplace_back();
  
  flame_graph(flames, self);

  for(const auto& it: flames) {
    assert(!it.stack.empty());

    bool first = true;
    for(auto id: it.stack) {
      if(first) { first = false; }
      else {
        out << ";";
      }
      
      out << id;
    };

    out << " " << it.count << '\n';
  }
}




void trace_timeline(std::ostream& out, const timeline& self) {

  const auto quote = [&](std::string what) {
    return std::string("\"" + what + "\"");
  };
  
  const auto attr = [&](auto name, auto value) -> std::ostream& {
    return out << quote(name) << ": " << value;
  };

  out << "{\n";
  out << quote("traceEvents") << ": [\n";
  bool first = true;
  for(event ev: self) {
    if(first) { first = false; }
    else  {
      out << ",";
    }
    out << "{";
    attr("pid", 1) << ", ";
    attr("tid", self.thread) << ", ";
    attr("ph", ev.kind == event::BEGIN ? quote("B") : quote("E")) << ", ";
    attr("name", quote(ev.id)) << ", ";
    attr("ts", ev.timestamp());
    out << "}\n";
  }
  out << "]\n";
  out << "}\n";  
  
// {
// "traceEvents": [
// { "pid":1, "tid":1, "ts":87705, "dur":956189, "ph":"X", "name":"Jambase", "args":{ "ms":956.2 } },
// { "pid":1, "tid":1, "ts":128154, "dur":75867, "ph":"X", "name":"SyncTargets", "args":{ "ms":75.9 } },
// { "pid":1, "tid":1, "ts":546867, "dur":121564, "ph":"X", "name":"DoThings", "args":{ "ms":121.6 } }
// ],
// "meta_user": "aras",
// "meta_cpu_count": "8"
// }
  
}

////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>

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


int main(int argc, char** argv) {
  work();

  // for(auto& events: timeline::all()) {
  //   report::write_header(std::cout);
  //   report(call(*events).simplify()).write(std::cout);

  //   events->clear();
  // }

  // for(auto& events: timeline::all()) {
  //   flame_graph(std::cout, call(*events).simplify());
  //   events->clear();
  // }

  if(argc > 1) {
    std::ofstream out(argv[1]);
    for(auto* events: timeline::all()) {
      trace_timeline(out, *events);
      events->clear();
    }
  }
  
  return 0;
}
