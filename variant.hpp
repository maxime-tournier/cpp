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
    static inline constexpr std::size_t index(const H* ) { return start; }

    using select<start + 1, T...>::cast;
    static inline constexpr const H& cast(const H& value) { return value; }
    static inline constexpr H&& cast(H&& value) { return std::move(value); }    
    
  };

  
  template<std::size_t start> struct select<start> {
    static inline constexpr void index();
    static inline constexpr void cast();    
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

protected:
  using index_type = std::size_t;
  index_type index;

private:
  
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


  struct compare {

    template<class U>
    bool operator()(const U& self, const variant& other) const {
      return self == other.get<U>();
    }

  };

  struct less {

    template<class U>
    bool operator()(const U& self, const variant& other) const {
      return self < other.get<U>();
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
    if( R != index ) {
      throw bad_cast();
    }
    return res;
  }


  template<class U, index_type R = type_index<U>()>
  U& get() {
    U& res = reinterpret_cast<U&>(storage);
    assert(R == index);
    return res;
  } 


  template<class U, index_type R = type_index<U>() >
  const U& get() const {
    const U& res = reinterpret_cast<const U&>(storage);
    assert(R == index);
    return res;
  }
  

  template<class U, index_type R = type_index<U>()>
  U* get_if() {
    if(!is<U>()) return nullptr;
    return &get<U>();
  } 

  template<class U, index_type R = type_index<U>()>
  const U* get_if() const {
    if(!is<U>()) return nullptr;
    return &get<U>();
  } 
  
  

  
  template<class U, index_type R = type_index<U>() >
  bool is() const noexcept {
    return R == index;
  }
  
  variant(const variant& other) noexcept 
    : index(other.index) {
    static constexpr bool skip[] = {std::is_trivially_copy_constructible<T>::value...};
    
    if(skip[index]) {
      storage = other.storage;
    } else {
      apply( copy_construct(), other );
    }
  }

  variant(variant&& other) noexcept
    : index(other.index) {
    static constexpr bool skip[] = {std::is_trivially_move_constructible<T>::value...};
    
    if(skip[index]) {
      storage = std::move(other.storage);
    } else {
      apply( move_construct(), std::move(other) );
    }
  }
  
  ~variant() noexcept {
    static constexpr bool dont_skip[] = {!std::is_trivially_destructible<T>::value...};
    
    if(dont_skip[index]) {
      apply( destruct() );
    }
    
  }


  variant& operator=(const variant& other) noexcept {
    if(index == other.index) {
      apply(copy(), other);
    } else {
      this->~variant();
      index = other.index;      
      apply(copy_construct(), other);
    }
    
    return *this;
  }

  bool operator==(const variant& other) const {
    if(type() != other.type()) return false;
    return map<bool>(compare(), other);
  }

  bool operator!=(const variant& other) const {
    return !operator==(other);
  }

  
  bool operator<(const variant& other) const {
    return index < other.index || (index == other.index && map<bool>(less(), other));
  }

  
  
  variant& operator=(variant&& other)  noexcept{
    
    if(index == other.index) {
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
