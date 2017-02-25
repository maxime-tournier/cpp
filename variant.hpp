#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <type_traits>
#include <memory>

#include <iostream>

namespace impl {
  template<std::size_t, class ... T> struct select;

  template<std::size_t total_size> struct select<total_size> { };

  template<std::size_t total_size, class H, class ...T>
  struct select<total_size, H, T...> : select<total_size, T...> {

	// friend function so that adl is used 
	friend constexpr std::size_t index_of(const select&, const H* self) {
	  return total_size - (1 + sizeof...(T));
	}
  
};
}

// a variant shared pointer type
template<class ... T>
class variant {
  
  using ptr_type = std::shared_ptr<void>;
  ptr_type ptr;
  
  // TODO infer largest index_type based on data alignement?
  using index_type = std::uintptr_t;
  index_type index;

  using select_type = impl::select<sizeof...(T), T...>;

  template<class Variant, class U, class Visitor, class ... Args>
  static void thunk(Variant& self, Visitor&& visitor, Args&& ... args) {
	U* ptr = reinterpret_cast<U*>(self.ptr.get());
	std::forward<Visitor>(visitor)(ptr, std::forward<Args>(args)...);
  }

  
public:
  
  variant() : ptr(nullptr), index(0) { }

  explicit operator bool() {
	return ptr;
  }

  
  template<class U>
  variant( const std::shared_ptr<U>& u)
	: ptr( u ),
	  index( index_of(select_type(), u.get()) ) {
	
  }


  template<class U>
  variant( std::shared_ptr<U>&& u)
	: ptr( std::move(u) ),
	  index( index_of(select_type(), u.get()) ) {
	
  }


  variant(const variant& ) = default;
  variant(variant&& ) = default;  

  variant& operator=(const variant& ) = default;
  variant& operator=(variant&& ) = default;  
  
  
  // template<class Visitor, class ... Args>
  // void apply(Visitor&& visitor, Args&& ... args) {
  // 	using thunk_type = void (*)(variant&, Visitor&&, Args&& ...);
	
  // 	static constexpr thunk_type dispatch [] = {
  // 	  thunk<variant, T, Visitor, Args...>...
  // 	};
	
  // 	dispatch[index](*this, std::forward<Visitor>(visitor),
  // 					std::forward<Args>(args)...);
  // }


  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) const {
	using thunk_type = void (*)(const variant&, Visitor&&, Args&& ...);
	
	static constexpr thunk_type dispatch [] = {
	  thunk<const variant, T, Visitor, Args...>...
	};
	
	dispatch[index](*this, std::forward<Visitor>(visitor),
					std::forward<Args>(args)...);
  }
  
  
};


#endif

