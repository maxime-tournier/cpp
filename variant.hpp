#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <type_traits>
#include <memory>
#include <cassert>
#include <stdexcept>

template<class ... T> class variant;


namespace impl {
  template<std::size_t start, class ... T>
  struct select;

  template<std::size_t start, class H, class ... T>
  struct select<start, H, T...> : select<start + 1, T...> {

    using select<start + 1, T...>::index;
    static constexpr std::size_t index(const H* ) { return start; }

    using select<start + 1, T...>::cast;
    static constexpr const H& cast(const H& value) { return value; }
    static constexpr H&& cast(H&& value) { return std::move(value); }    
    
  };

  template<std::size_t start> struct select<start> {
    static void index();
    static void cast();    
  };


  template<class Variant, class Self, class Signature>
  struct dispatch;

  template<class ... T, class Self, class Visitor, class Ret, class ... Args>
  struct dispatch<variant<T...>, Self, Ret (Visitor, Args...) > {
  
    template<class U>
    static Ret call_thunk(Self& self, Visitor&& visitor, Args&& ... args) {
      return std::forward<Visitor>(visitor)(self.template get<U>(), std::forward<Args>(args)...);
    }
  
    using thunk_type = Ret (*)(Self& self, Visitor&& visitor, Args&& ... args);
    static constexpr thunk_type table[] = { call_thunk<T>... };
  };

  template<class ... T, class Self, class Visitor,  class Ret, class ... Args>
  constexpr typename dispatch<variant<T...>, Self, Ret (Visitor, Args...) >::thunk_type
  dispatch<variant<T...>, Self, Ret (Visitor, Args...) >::table[];

  
}




template<class ... T>
class variant {

  using storage_type = typename std::aligned_union<0, T...>::type;
  storage_type storage;

  using index_type = std::size_t;
  index_type index;
  
  using select_type = impl::select<0, T...>;


  template<class U>
  void construct(U&& value) {
    new (&storage) typename std::decay<U>::type(std::forward<U>(value));
  }
  
  struct copy_construct {
    template<class U>
    void operator()(U& self, const variant& other) const {
      new (&self) U(other.template get<U>());
    }
  };


  struct copy {
    template<class U>
    void operator()(U& self, const variant& other) const {
      self = other.template get<U>();
    }
  };


  struct move {
    template<class U>
    void operator()(U& self, variant&& other) const {
      self = std::move(other.template get<U>());
    }
  };
  
  
  struct move_construct {
    template<class U>
    void operator()(U& self, variant&& other) const {
      new (&self) U(std::move(other.template get<U>()));
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

  template<class U, index_type R = select_type::index( ((typename std::decay<U>::type*)0) ) >
  static constexpr index_type type_index() { return R; }
  
  template<class U, index_type R = type_index<U>() >
  U& cast() {
    U& res = reinterpret_cast<U&>(storage);
    if( R != index ) throw bad_cast();
    return res;
  }

  
  template<class U, index_type R = type_index<U>() >
  const U& cast() const {
    const U& res = reinterpret_cast<const U&>(storage);
    if(R != index ) {
      throw std::runtime_error("bad cast");
    }
    return res;
  }


  template<class U, index_type R = type_index<U>()>
  U& get() noexcept {
    U& res = reinterpret_cast<U&>(storage);
    assert(R == index);
    return res;
  }

  template<class U, index_type R = type_index<U>() >
  const U& get() const noexcept {
    const U& res = reinterpret_cast<const U&>(storage);

    assert(R == index);
    return res;
  }
  
  
  template<class U, index_type R = type_index<U>() >
  bool is() const noexcept {
    return R == index;
  }
  
  variant(const variant& other) noexcept 
    : index(other.index) {
    apply( copy_construct(), other );
  }

  variant(variant&& other) noexcept
    : index(other.index) {
    apply( move_construct(), std::move(other) );
  }
  
  ~variant() noexcept {
    static constexpr bool skip_destructor[] = {std::is_trivially_destructible<T>::value...};    
    if(!skip_destructor[index]) apply( destruct() );
  }


  variant& operator=(const variant& other) noexcept {
    if(type() == other.type()) {
      apply( copy(), other );
    } else {
      this->~variant();
      index = other.index;
      apply( copy_construct(), other );
    }
    
    return *this;
  }


  variant& operator=(variant&& other)  noexcept{
    if(type() == other.type()) {
      apply( move(), std::move(other) );
    } else {
      this->~variant();      
      index = other.index;      
      apply( move_construct(), std::move(other) );
    }
    
    return *this;
  }
  
  
  template<class U, index_type R = type_index<U>() >
  variant(U&& value) noexcept
    : index( R ) {
    construct( select_type::cast( std::forward<U>(value)) );
  }


  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) {

    impl::dispatch<variant, variant, void (Visitor, Args...)>::table[index]
      (*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
    
  }

  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) const {

    impl::dispatch<variant, const variant, void (Visitor, Args...)>::table[index]
      (*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);

  }


  template<class Return, class Visitor, class ... Args>
  Return map(Visitor&& visitor, Args&& ... args) const {

    return impl::dispatch<variant, const variant, Return (Visitor, Args...) >::table[index]
      (*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
    
  }

  
};




#endif
