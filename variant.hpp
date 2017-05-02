#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <type_traits>
#include <memory>
#include <cassert>

namespace impl {
  template<std::size_t start, class ... T>
  struct select;

  template<std::size_t start, class H, class ... T>
  struct select<start, H, T...> : select<start + 1, T...> {

    using select<start + 1, T...>::index;
    static constexpr std::size_t index(const H& ) { return start; }

    using select<start + 1, T...>::cast;
    static const H& cast(const H& value) { return value; }
    static H&& cast(H&& value) { return std::move(value); }    
    
  };

  template<std::size_t start> struct select<start> {
    static void index();
    static void cast();    
  };
}


template<class ... T>
class variant {

  using storage_type = typename std::aligned_union<0, T...>::type;
  storage_type storage;

  using index_type = std::size_t;
  index_type index;
  
  using select_type = impl::select<0, T...>;

  template<class U, class Ret, class Variant, class Visitor, class ... Args>
  static Ret call_thunk(Variant& self, Visitor&& visitor, Args&& ... args) {
    return std::forward<Visitor>(visitor)(self.template unsafe<U>(), std::forward<Args>(args)...);
  }

  template<class U>
  void construct(U&& value) {
    new (&storage) typename std::decay<U>::type(std::forward<U>(value));
  }
  
  struct copy_construct {
    template<class U>
    void operator()(U& self, const variant& other) const {
      new (&self) U(other.template unsafe<U>());
    }
  };


  struct copy {
    template<class U>
    void operator()(U& self, const variant& other) const {
      self = other.template unsafe<U>();
    }
  };


  struct move {
    template<class U>
    void operator()(U& self, variant&& other) const {
      self = std::move(other.template unsafe<U>());
    }
  };
  
  
  struct move_construct {
    template<class U>
    void operator()(U& self, variant&& other) const {
      new (&self) U(std::move(other.template unsafe<U>()));
    }
  };


  
  struct destruct {
    template<class U>
    void operator()(U& self) const {
      self.~U();
    }
  };



public:
  std::size_t type() const { return index; }

  struct bad_cast : std::runtime_error {
    bad_cast() : std::runtime_error("bad_cast") { }
  };
  
  template<class U>
  U& get() {
    U& res = reinterpret_cast<U&>(storage);
    if( select_type::index(res) != index ) throw bad_cast();
    return res;
  }
  
  struct copy {
    template<class U>
    void operator()(U& self, const variant& other) const {
      new (&self) U(other.template get<U>());
    }
  };

  struct move {
    template<class U>
    void operator()(U& self, variant&& other) const {
      new (&self) U(std::move(other.template get<U>()));
    }
  };

  template<class U>
  const U& get() const {
    const U& res = reinterpret_cast<const U&>(storage);
    if( select_type::index(res) != index ) {
      std::clog << select_type::index(res) << " vs. " << index << std::endl;
      throw std::runtime_error("bad cast");
    }
    return res;
  }


  template<class U>
  U& unsafe() {
    U& res = reinterpret_cast<U&>(storage);
    assert(select_type::index(res) == index);
    return res;
  }

  template<class U>
  const U& unsafe() const {
    const U& res = reinterpret_cast<const U&>(storage);
    assert(select_type::index(res) == index);
    return res;
  }
  
  
  template<class U, int R = select_type::index( *(U*)0 )>
  bool is() const {
    return R == index;
  }
  
  
  variant(const variant& other)
    : index(other.index) {
    apply( copy_construct(), other );
  }

  variant(variant&& other)
    : index(other.index) {
    apply( move_construct(), std::move(other) );
  }
  
  ~variant() {
    apply( destruct() );

    // const int expand[] = { ((std::clog << typeid(T).name() << " " << sizeof() << std::endl), 0)... };
  }

  
  template<class U>
  variant(U&& value)
    : index( select_type::index(value) ) {
    construct( select_type::cast(value) );
  }

  variant& operator=(const variant& other) {
    if(type() == other.type()) {
      apply( copy(), other );
    } else {
      apply( destruct() );
      apply( copy_construct(), other );
    }
    
    return *this;
  }


  variant& operator=(variant&& other) {
    if(type() == other.type()) {
      apply( move(), std::move(other) );
    } else {
      apply( destruct() );
      apply( move_construct(), std::move(other) );
    }
    
    return *this;
  }
  
  ~variant() {
    apply( destruct() );
  }
  
  
  // TODO move constructors from values ?
  template<class U, class = decltype( select_type::index( std::declval<const U&>() ) ) >
  variant(const U& value)
    : index( select_type::index(value) ) {
    construct( select_type::cast(value) );
  }
  
  
  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) {
    using thunk_type = void (*)(variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      call_thunk<T, void, variant, Visitor, Args...>...
    };
    
    thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
  }

  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) const {
    using thunk_type = void (*)(const variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      call_thunk<T, void, const variant, Visitor, Args...>...
    };
    
    thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
  }


  template<class Visitor, class ... Args>
  variant map(Visitor&& visitor, Args&& ... args) const {
    using thunk_type = variant (*)(const variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      call_thunk<T, variant, const variant, Visitor, Args...>...
    };
    
    return thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
  }

  
}; 


#endif
