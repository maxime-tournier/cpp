#ifndef RTTI_HPP
#define RTTI_HPP

#include <typeindex>

namespace rtti {
  template<class Base>
  class base {
	const std::type_index type;
  protected:  
  
	template<class Derived>
	base(Derived* ) : type( typeid(Derived) ) { }

  public:
  
	using base_type = Base;
	std::type_index get_type() const { return type; }
  
  };


  template<class Derived>
  class derived {

  public:

	template<class Self = Derived>
	static bool classof(const typename Self::base_type* obj) {
	  return obj->get_type() == typeid(Derived);
	}
	
  };

  // note: only down-casts are supported
  template<class Derived, class Base>
  static bool isa(const Base* obj) {
	return Derived::classof(obj);
  }

  template<class Derived, class Base>
  static Derived* cast(Base* obj) {
	return Derived::classof(obj) ? static_cast<Derived*>(obj) : nullptr;
  }
  
}


#endif
