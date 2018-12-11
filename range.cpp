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

  // expand a tuple into a continuation
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

  // type erasure using "impossibly fast delegates"
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
    
    any(const any&) = delete;
    any(any&&) = default;  

    any& operator=(const any&) = delete;
    any& operator=(any&&) = default;

    template<class Range>
    any(const Range& range):
      storage(new Range(range), deleter<Range>),
      get_ptr(thunk<Range, T, T (Range::*)() const, &Range::get>),
      bool_ptr(thunk<Range, bool, bool (Range::*)() const, &Range::operator bool>),
      next_ptr(thunk<Range, void, void (Range::*)(), &Range::next>) {
      
    }
 
  };

  template<class Func, class Arg>
  struct apply_traits {
    using type = typename std::result_of<Func(const Arg&)>::type;
    
    static type apply(const Func& func, const Arg& arg) { return func(arg); }
  };

  template<class Func, class ... Args>
  struct apply_traits<Func, std::tuple<Args...>> {
    using type = typename std::result_of<Func(const Args&...)>::type;
    
    static type apply(const Func& func, const std::tuple<Args...>& arg) {
      return tuple::expand(arg, func);
    }
  };

  template<class Func, class LHS, class RHS>
  struct apply_traits<Func, std::pair<LHS, RHS>> {
    using type = typename std::result_of<Func(const LHS&, const RHS&)>::type;
    
    static type apply(const Func& func, const std::pair<LHS, RHS>& arg) {
      return func(arg.first, arg.second);
    }
  };


  // void argument tag class
  struct void_tag { };

  // when forming (arg, void_tag{}), if arg is void then the standard operator
  // comma will be used, which will return void_tag. otherwise, this overload
  // will be picked, which returns the actual argument type.
  template<class T>
  static const T& operator,(const T& rhs, void_tag) { return rhs; }
  
  template<class Func>
  struct apply_traits<Func, void> {
    using type = typename std::result_of<Func()>::type;

    static type apply(const Func& func, void_tag) {
      return func();
    }
  };
  
  
  // functor map
  template<class Range, class Func>
  struct map_range {
    Range range;
    const Func func;

    using traits = apply_traits<Func, typename Range::value_type>;
    using value_type = typename traits::type;
    
    void next() { range.next(); }
    explicit operator bool() const { return bool(range); }
    value_type get() const { return traits::apply(func, (range.get(), void_tag{})); }
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

    using traits = apply_traits<Pred, value_type>;
    
    filter_range(const Range& range, const Pred& pred):
      range(range),
      pred(pred) {
      // pull range until predicate matches
      while(this->range && !traits::apply(pred, (this->range.get(), void_tag{}))) {
        this->range.next();
      }
    }

    explicit operator bool() const { return bool(range); }
    value_type get() const { return range.get(); }

    void next() {
      // pull range until pred is met
      for(range.next(); range && !traits::apply(pred, (range.get(), void_tag{})); range.next()) { }
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
    bool consumed;
    
    single_range(const T& value): value(value), consumed(false) { }

    using value_type = T;
    
    explicit operator bool() const { return !consumed; }
    const T& get() const { return value; }
    void next() { consumed = true; }
  };

  template<class T>
  static single_range<T> single(const T& value) { return {value}; }


  // monadic bind for range sequencing
  template<class Range, class Func>
  struct bind_range {
    using impl_type = map_range<Range, Func>;
    impl_type impl;

    using result_type = typename impl_type::value_type;
    
    // note: we need to store the impl result (otherwise side-effects will
    // happen on temporaries)
    using storage_type = typename std::aligned_union<0, result_type>::type;
    storage_type storage;

    result_type& current() { return reinterpret_cast<result_type&>(storage); }
    const result_type& current() const {
      return reinterpret_cast<const result_type&>(storage);
    }    

    void find() {
      // find next valid state from a valid impl
      while(impl) {
        new (&storage) result_type(impl.get());
        if(!current()) {
          current().~result_type();
          impl.next();
        } else {
          break;
        }
      }
    }
    
    bind_range(Range range, Func func):
      impl(map(range, func)) {
      find();      
    }
    
    explicit operator bool() const {
      return bool(impl);
    }    
    
    using value_type = typename result_type::value_type;
    value_type get() const { return current().get(); }
    
    void next() {
      current().next();
      
      if(!current()) {
        current().~result_type();
        impl.next();

        find();
        // note: if impl is exhausted, current is destroyed
      }
    }

    ~bind_range() {
      if(impl) {
        // only need to destroy if impl is not exhausted
        current().~result_type();
      }
    }
    
  };


  template<class Range, class Func>
  static bind_range<Range, Func> operator>>=(Range range, Func func) {
    return {range, func};
  }
  
  
  // vanilla sequencing
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
    bool cond;
    guard(bool cond): cond(cond) { }

    using value_type = void;
    
    void next() { cond = false; };
    explicit operator bool() const { return cond; }
    void get() const { };
  };

  /////////////////////////////////////////////////////////////////////////////////
  // utils
  
  // left fold a range
  template<class Init, class Range, class Func>
  static typename std::result_of<Func(Init&&, typename std::decay<Range>::type::value_type)>::type
  foldl(Init&& init, Range&& range, const Func& func) {
    if(!range) return init;
    const auto value = range.get();
    range.next();
    return foldl(func(init, value), range, func);
  }

  
  // right fold a range (TODO check type)
  template<class Init, class Range, class Func>
  static typename std::result_of<Func(typename std::decay<Range>::type::value_type, Init&&)>::type
  foldr(Init&& init, Range&& range, const Func& func) {
    if(!range) return init;
    const auto value = range.get();
    range.next();
    return func(value, foldr(init, range, func));
  }

  // iterate a range
  template<class Range, class Func>
  static void iter(Range range, Func func) {
    while(range) {
      apply_traits<Func, typename Range::value_type>::apply(func, (range.get(), void_tag{}));
      range.next();
    }
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // actual concrete ranges
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
  static iterator_range<typename Container::const_iterator>
  forward(const Container& self) {
    return {self.begin(), self.end()};
  }

  // backward range on container
  template<class Container>
  static iterator_range<typename Container::const_reverse_iterator>
  backward(const Container& self) {
    return {self.rbegin(), self.rend()};
  }
  
  
  // counting range
  template<class T>
  struct count_range {
    T curr;
    const T step;
    using value_type = T;
    
    explicit operator bool() const { return true; }
    void next() { curr += step; }
    const T& get() const { return curr; }
  };

  template<class T=std::size_t>
  static count_range<T> count(T from=0, T step=1) { return {from, step}; }


  // semi-open interval
  template<class T>
  struct interval_range {
    T curr;
    const T last;
    const T step;

    using value_type = T;
    
    explicit operator bool() const { return curr < last; }
    void next() { curr += step; }
    const T& get() const { return curr; }
  };

  template<class T=std::size_t>
  static interval_range<T> interval(T from, T to, T step=1) {
    return {from, to, step};
  }

  template<class T=std::size_t>
  static interval_range<T> upto(T to, T step=1) { return {0, to, step}; }
  
  
  // enumerate
  template<class Range>
  static auto enumerate(const Range& range) -> decltype(zip(count(), range)) {
    return zip(count(), range);
  }
  


}



int main(int, char** ) {
  const std::size_t n = 10;

  // const auto range = range::upto(n) >>= [=](std::size_t x) {
  //   return range::interval(x, n) >>= [=](std::size_t y) {
  //     return range::interval(y, n) >>= [=](std::size_t z) {
  //       return range::guard(x * x + y * y == z * z) >>= [&] {
  //         return range::single(std::make_tuple(x, y, z));
  //       };
  //     };
  //   };
  // };


  // iter(range, [](std::size_t x, std::size_t y, std::size_t z) {
  //   std::cout << "x: " << x << ", y: " << y << ", z: " << z << std::endl;
  // });


  const auto range = zip(range::upto(n), range::upto(n)) >>= [&](std::size_t x, std::size_t y) {
    return range::single(x + y);
  };

  
  // const auto range = range::upto(n) >>= [](std::size_t x) {
  //   return range::guard(x < 5) >>= [=](bool) {
  //     return range::single(x);
  //   };
  // };


  iter(range, [](std::size_t x) {
      std::cout << "x: " << x << std::endl;
    });
  
  
  return 0;
}
