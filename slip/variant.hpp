#ifndef SLIP_VARIANT_HPP
#define SLIP_VARIANT_HPP

namespace detail {

  // pick the right index/constructor from a value
  template<std::size_t size, class ... T> struct selector;

  template<std::size_t size> struct selector<size> {
    static void index() { }
    static void construct() { }    
  };
  
  template<std::size_t size, class H, class ... T>
  struct selector<size, H, T...> : selector<size, T...> {

    using selector<size, T...>::index;
    static constexpr std::integral_constant<std::size_t, size - (1 + sizeof...(T))> index(const H& ) {
      return {};
    }
    
    using selector<size, T...>::construct;
    template<class U>
    static void construct(void* where, const H&, U&& value) {
      new (where) std::unique_ptr<H>( new H(std::forward<U>(value)));
    }
    
  };

  // visitor return type
  template<class Visitor, class = void>
  struct visitor {
    using type = void;
  };

  template<class> using require = void;
  
  template<class Visitor>
  struct visitor< Visitor, require< typename Visitor::type > > {
    using type = typename Visitor::type;
  };
  
}

template<class ... T>
class variant {
  using selector_type = detail::selector<sizeof...(T), T...>;
  
  using storage_type = typename std::aligned_union< 0, std::unique_ptr<T>...>::type;
  storage_type storage;
  
  using index_type = unsigned int;
  index_type index;

  template<class U>
  const std::unique_ptr<U>& ptr() const { return reinterpret_cast< const std::unique_ptr<U>&>(storage); }

  template<class U>
  std::unique_ptr<U>& ptr() { return reinterpret_cast< std::unique_ptr<U>&>(storage); }

  
  template<class Ret, class U, class Visitor, class Variant, class ... Args>
  static Ret apply_thunk(Visitor&& visitor, Variant& self, Args&& ... args) {
    return std::forward<Visitor>(visitor)(self.template get<U>(), std::forward<Args>(args)...);
  };
  

  template<class U> void construct() { new (&storage) std::unique_ptr<U>(); }
  template<class U> void destruct() { ptr<U>().reset(); }

  template<class U> void copy(const variant& other) { ptr<U>().reset( new U(other.get<U>())); }
  template<class U> void move(variant&& other) { ptr<U>() = std::move( other.ptr<U>() ); }

  template<class U> void copy_construct(const variant& other) { construct<U>(); copy<U>(other); }
  template<class U> void move_construct(variant&& other) {construct<U>(); move<U>(std::move(other)); }
  
  template<class U>
  using require_unrelated =
    typename std::enable_if< !std::is_same<variant, typename std::decay<U>::type>::value>::type;


  template<class U>
  static constexpr std::size_t index_of() {
    return decltype( selector_type::index( std::declval<U>() ) )::value;
  }
  
public:

  template<class U, class = require_unrelated<U> >
  variant(U&& value) : index( index_of<U>() ) {
    selector_type::construct( (void*)&storage, value, std::forward<U>(value));
  }

  // get value, throw if type is wrong
  template<class U, index_type expected = index_of<U>(), class = require_unrelated<U> >
  const U& get() const {
    if(index != expected) throw std::bad_cast();
    return *ptr<U>();
  }
  
  template<class U, index_type expected = index_of<U>(), class = require_unrelated<U> >
  U& get() {
    if(index != expected) throw std::bad_cast();
    return *ptr<U>();
  }

  // get value, nullptr if type is wrong
  template<class U, index_type expected = index_of<U>(), class = require_unrelated<U> >
  const U* get_if() const {
    if(index != expected) return nullptr;
    return ptr<U>().get();
  }
  
  template<class U, index_type expected = index_of<U>(), class = require_unrelated<U> >
  U* get_if() {
    if(index != expected) return nullptr;
    return ptr<U>().get();
  }

  // test value type
  template<class U, index_type expected = index_of<U>(), class = require_unrelated<U> >
  bool is() const { return index == expected; }
  

  // the magic five
  ~variant() {
    using thunk_type = void (variant::*)();
    static const thunk_type thunk [] = { &variant::destruct<T>... };
    (this->*thunk[index])();
  };
  

  variant(const variant& other) : index(other.index) {
    using thunk_type = void (variant::*)(const variant&);
    static const thunk_type thunk [] = { &variant::copy_construct<T>... };
    (this->*thunk[index])(other);
  }

  variant(variant&& other) : index(other.index) {
    using thunk_type = void (variant::*)(variant&&);
    static const thunk_type thunk [] = { &variant::move_construct<T>... };
    (this->*thunk[index])(std::move(other));
    
  }


  variant& operator=(const variant& other) {
    if(this == &other) return *this;

    using thunk_type = void (variant::*)(const variant&);
    
    if( index == other) {
      static const thunk_type thunk [] = { &variant::copy<T>... };
      (this->*thunk[index])(other);
    } else {
      this->~variant();
      static const thunk_type thunk [] = { &variant::copy_construct<T>... };
      (this->*thunk[index])(other);      
    }

  }

  variant& operator=(variant&& other) {
    if(this == &other) return *this;

    using thunk_type = void (variant::*)(variant&&);
    
    if( index == other) {
      static const thunk_type thunk [] = { &variant::move<T>... };
      (this->*thunk[index])(std::move(other));
    } else {
      this->~variant();
      static const thunk_type thunk [] = { &variant::move_construct<T>... };
      (this->*thunk[index])(std::move(other));      
    }

  }

  // apply visitor
  template<class Visitor, class ... Args>
  typename detail::visitor<Visitor>::type apply(Visitor&& visitor, Args&& ... args) const {
    using return_type = typename detail::visitor<Visitor>::type;
    using thunk_type = return_type (*)(Visitor&&, const variant&, Args&&...);
    
    static const thunk_type thunk[] = { apply_thunk<return_type, T, Visitor, const variant, Args...>... };
    return thunk[index](std::forward<Visitor>(visitor), *this, std::forward<Args>(args)...);
  }
  
};

#endif
