// -*- compile-command: "c++ -O3 -o range range.cpp -std=c++11 -lstdc++" -*-


#include <vector>
#include <memory>
#include <algorithm>

#include <iostream>


namespace type {

  // foldl
  template<template<class, class> class Func, class Init, class ... Args>
  struct foldl;

  template<template<class, class> class Func, class Init>
  struct foldl<Func, Init> {
    using type = Init;
  };

  template<template<class, class> class Func, class Init, class H, class ... T>
  struct foldl<Func, Init, H, T...> {
    using type = typename foldl<Func, typename Func<Init, H>::type, T...>::type;
  };

  // foldr
  template<template<class, class> class Func, class Init, class ... Args>
  struct foldr;
  
  template<template<class, class> class Func, class Init>
  struct foldr<Func, Init> {
    using type = Init;
  };

  template<template<class, class> class Func, class Init, class H, class ... T>
  struct foldr<Func, Init, H, T...> {
    using type = typename Func<H, typename foldr<Func, Init, T...>::type>::type;
  };


  // tuple numbering
  template<std::size_t ...> struct indices { };

  template<std::size_t I>
  using integer_constant = std::integral_constant<std::size_t, I>;

  template<class LHS, class RHS>
  struct rank;
  
  template<std::size_t ... Js, class RHS>
  struct rank<indices<Js...>, RHS> {
    using type = indices<Js..., sizeof...(Js)>;
  };
  
  // index sequence for a type sequence
  template<class ... T>
  using indices_for = typename foldl<rank, indices<>, T...>::type;
}

namespace tuple {
  
  template<class ... T, class Func, std::size_t ... Is>
  typename std::result_of<Func(const T&...)>::type expand(const std::tuple<T...>& args, const Func& func,
                                                    type::indices<Is...>) {
    return func(std::get<Is>(args)...);
  }

  template<class ... T, class Func, std::size_t ... Is>
  typename std::result_of<Func(T&...)>::type expand(std::tuple<T...>& args, const Func& func, 
                                                    type::indices<Is...>) {
    return func(std::get<Is>(args)...);
  }
  
  
  template<class ... T, class Func>
  typename std::result_of<Func(const T&...)>::type expand(const std::tuple<T...>& args, const Func& func) {
    return expand(args, func, type::indices_for<T...>());
  }


  template<class ... T, class Func>
  typename std::result_of<Func(T&...)>::type expand(std::tuple<T...>& args, const Func& func) {
    return expand(args, func, type::indices_for<T...>());
  }
  
}


namespace range {

  // type erasure
  template<class T>
  class any {

    template<class Range>
    static void deleter(void* self) {
      delete reinterpret_cast<Range*>(self);
    };
    
    std::unique_ptr<void, void (*)(void*)> storage;

    T (*get_ptr)(const void*);
    bool(*bool_ptr)(const void*);
    void (*next_ptr)(void*);

    template<class Range, class Ret, class U, U method>
    static Ret thunk(void* self) {
      return (reinterpret_cast<Range*>(self)->*method)();
    }

    template<class Range, class Ret, class U, U method>
    static Ret thunk(const void* self) {
      return (reinterpret_cast<const Range*>(self)->*method)();
    }
    
  public:

    using value_type = T;

    // range api
    void next() { next_ptr(storage.get()); }
    explicit operator bool() const { return bool_ptr(storage.get()); }
    T get() const { return get_ptr(storage.get()); }
    
    any(const any&) = default;
    any(any&&) = default;  
  
    template<class Range>
    any(const Range& range):
      storage(new Range(range), deleter<Range>),
      get_ptr(thunk<Range, T, T (Range::*)() const, &Range::get>),
      bool_ptr(thunk<Range, bool, bool (Range::*)() const, &Range::operator bool>),
      next_ptr(thunk<Range, void, void (Range::*)(), &Range::next>) {
      
    }
 
  };


  // functor map
  template<class Range, class Func>
  struct map_range {
    Range range;
    const Func func;

    using value_type = typename std::result_of<Func(typename Range::value_type)>::type;
    
    void next() { range.next(); }
    explicit operator bool() const { return bool(range); }
    value_type get() const { return func(range.get());}
    
  };

  
  template<class Range, class Func>
  static map_range<Range, Func> map(const Range& range, const Func& func) {
    return {range, func};
  }


  // filtering combinator
  template<class Range, class Pred>
  struct filter_range {
    Range range;
    const Pred pred;

    using value_type = typename Range::value_type;
    
    filter_range(const Range& range, const Pred& pred):
      range(range),
      pred(pred) {
      while(this->range && !pred(this->range.get())) {
        this->range.next();
      }
    }

    explicit operator bool() const { return bool(range); }
    value_type get() const { return range.get(); }

    void next() {
      for(range.next(); range && !pred(range.get()); range.next()) { }
    }
    
  };


  template<class Range, class Pred>
  static filter_range<Range, Pred> filter(const Range& range, const Pred& pred) {
    return {range, pred};
  }


  // iterate multiple ranges in parallel
  template<class ... Ranges>
  struct zip_range {
    std::tuple<Ranges...> ranges;
    zip_range(const Ranges& ...ranges): ranges(ranges...) { }

    explicit operator bool() const {
      return tuple::expand(ranges, [](const Ranges& ... ranges) {
          const bool expand[] = {bool(ranges)...};
          return std::all_of(expand, expand + sizeof...(Ranges), [](bool self) { return self; });
        });
    }

    void next() {
      return tuple::expand(ranges, [](Ranges& ... ranges) {
          const int expand[] = {(ranges.next(), 0)...};
        });
    }

    using value_type = std::tuple<typename Ranges::value_type...>;
    
    value_type get() const {
      return tuple::expand(ranges, [](const Ranges& ... ranges) {
          return value_type(ranges.get()...);
        });
    }
    
  };


  template<class ... Ranges>
  static zip_range<Ranges...> zip(const Ranges& ... ranges) {
    return {ranges...};
  }


  // left fold
  template<class Init, class Range, class Func>
  static typename std::result_of<Func(Init&&, typename std::decay<Range>::type::value_type)>::type
  foldl(Init&& init, Range&& range, const Func& func) {
    if(!range) return init;
    const auto value = range.get();
    range.next();
    return foldl(func(init, value), range, func);
  }

  
  // right fold (TODO check type)
  template<class Init, class Range, class Func>
  static typename std::result_of<Func(typename std::decay<Range>::type::value_type, Init&&)>::type
  foldr(Init&& init, Range&& range, const Func& func) {
    if(!range) return init;
    const auto value = range.get();
    range.next();
    return func(value, foldr(init, range, func));
  }
  

  // repeat value
  template<class T>
  struct repeat_range {
    const T value;

    explicit operator bool() const { return true; }
    const T& get() const { return value; }
    void next() { }
  };

  template<class T>
  static repeat_range<T> repeat(const T& value) { return {value}; }


  // yield value once (monadic pure for range sequencing)
  template<class T>
  struct single_range {
    const T value;
    bool consumed = false;

    explicit operator bool() const { return !consumed; }
    const T& get() const { return value; }
    void next() { consumed = true; }
  };

  template<class T>
  static single_range<T> single(const T& value) { return {value}; }
  

  // monadic bind for range sequencing
  template<class Range, class Func>
  struct bind_range {
    map_range<Range, Func> impl;

    bind_range(Range range, Func func):
      impl(map(range, func)) { }
    
    using result_type = typename std::result_of<Func(typename Range::value_type)>::type;
    using value_type = typename result_type::value_type;
    value_type get() const { return impl.get().get(); }
    
    explicit bool operator() const { return impl && impl.get(); }

    void next() {
      if(impl.get()) impl.get().next();
      else impl.next();
    }
    
  };


  template<class Range, class Func>
  static bind_range<LHS, RHS> operator>>=(Range range, Func func) {
    return {range, func};
  }
  
  
  // sequencing
  template<class LHS, class RHS>
  struct sequence_range {
    LHS lhs;
    RHS rhs;

    void next() {
      if(lhs) lhs.next();
      else rhs.next();
    }
  
    explicit operator bool() const { return lhs || rhs; }
  
    using value_type = typename LHS::value_type;
    static_assert(std::is_same<value_type, typename RHS::value_type>::value, "derp");
  
    value_type get() const {
      if(lhs) return lhs.get();
      else return rhs.get();
    }
  
  };


  template<class LHS, class RHS>
  static sequence_range<LHS, RHS> seq(LHS lhs, RHS rhs) {
    return {lhs, rhs};
  }

  template<class LHS, class RHS>
  static sequence_range<LHS, RHS> operator>>(LHS lhs, RHS rhs) {
    return {lhs, rhs};
  }


  struct guard {
    const bool cond;
    guard(bool cond): cond(cond) { }
    
    void next() { };
    explicit operator bool() const { return cond; }
    void get() const { };
  };
  
  ////////////////////////////////////////////////////////////////////////////////
  // actual impls
  template<class Iterator>
  struct iterator_range {
    Iterator begin;
    const Iterator end;
    
    void next() { ++begin; }
    explicit operator bool() const { return begin != end; }
 
    using value_type = typename Iterator::value_type;
    value_type get() const { return *begin; }
  };

  template<class Iterator>
  static iterator_range<Iterator> from_iterators(Iterator begin, Iterator end) {
    return {begin, end};
  }


  // forward range on container
  template<class Container>
  static iterator_range<typename Container::const_iterator> forward(const Container& self) {
    return {self.begin(), self.end()};
  }

  // backward range on container
  template<class Container>
  static iterator_range<typename Container::const_reverse_iterator> backward(const Container& self) {
    return {self.rbegin(), self.rend()};
  }
  
  
  // counting range
  template<class T>
  struct count_range {
    T curr;
    const T step;
    using value_type = T;
    
    explicit operator bool() const { return true; }
    void next() { ++curr; }
    const T& get() const { return curr; }
  };

  template<class T=std::size_t>
  static count_range<T> count(T from=0, T step=1) { return {from, step}; }


  // enumerate
  template<class Range>
  static auto enumerate(const Range& range) -> decltype(zip(count(), range)) {
    return zip(count(), range);
  }
  

  // iterate a range
  template<class Range, class Func>
  static void iter(Range range, Func func) {
    while(range) {
      func(range.get());
      range.next();
    }
  }

  // convenience overload for zips
  template<class ... Ranges, class Func>
  static void iter(zip_range<Ranges...> range, Func func) {
    while(range) {
      tuple::expand(range.get(), func);
      range.next();
    }
  }
  
}



int main(int, char** ) {
  const std::size_t n = 10;

  range::any<std::size_t> range = range::count(0, n) >>= [&](std::size_t x) {
    return range::count(x, n) >>= [&](std::size_t y) {
      return range::count(y, n) >>= [&](std::size_t z) {
        return range::guard(x * x + y * y == z * z) >>= [&] {
          return range::single(std::make_tuple(x, y, z));
        };
      };
    };
  };

  
  foldl(std::clog, range::forward(data), [](std::ostream& out, double value) -> std::ostream& {
      return out << value << " ";
    }) << std::endl;

  foldr(std::clog, range::forward(data), [](double value, std::ostream& out) -> std::ostream& {
      return out << value << " ";
    }) << std::endl;
  
  
  return 0;
}
