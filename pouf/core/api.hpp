#ifndef POUF_API_HPP
#define POUF_API_HPP

#include <type_traits>
#include <memory>

#include <iostream>
#include <core/variant.hpp>


// an embedded constant variant to ease double dispatch
template<class ... T>
class api {

  // TODO should we store base pointer directly?
  using index_type = unsigned short;
  const index_type index, shift;
  
  using select_type = impl::select<sizeof...(T), T...>;

  template<class Variant, class U, class Visitor, class ... Args>
  static void thunk(Variant& self, Visitor&& visitor, Args&& ... args) {
	std::forward<Visitor>(visitor)(self.template ptr<U>(),
								   std::forward<Args>(args)...);
  }
  
  // now this is fucked up
  template<class U>
  U* ptr() const {
	return reinterpret_cast<U*>((char*)this - shift);
  }
  
public:

  index_type type() const { return index; }
  
  template<class C, class B>
  api(C* owner, const api (B::*member) )
  : index( index_of(select_type(), owner ) ),
	shift( reinterpret_cast<std::size_t>(&(((C*)0)->*member)) ) {
	assert(ptr<C>() == owner);
  }
	
  api(const api& ) = delete;
  api& operator=(const api& ) = delete;
  
  api(api&& ) = default;    
  api& operator=(api&& ) = default;  
  

  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) const {
	using thunk_type = void (*)(const api&, Visitor&&, Args&& ...);
	
	static constexpr thunk_type dispatch [] = {
	  thunk<const api, T, Visitor, Args...>...
	};
	
	dispatch[index](*this, std::forward<Visitor>(visitor),
					std::forward<Args>(args)...);
  }
  
};




#endif

