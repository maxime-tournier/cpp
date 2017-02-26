#ifndef RTTI_HPP
#define RTTI_HPP

#include <typeindex>

// llvm-style rtti using function pointers as type ids
namespace rtti {
  
  template<class Base>
  class base {
	
	using type_id = void (*)();
	const type_id type;
	
  protected:  

	template<class Derived>
	base(Derived* ) : type( &get_type_id<Derived> ) { }

	
  public:
  
	using base_type = Base;
	type_id get_type() const { return type; }

	template<class Derived>
	static void get_type_id() { };
  };


  template<class Derived>
  class derived {
  public:

	template<class Self = Derived, class Base>
	static bool classof(const Base* obj) {
	  return obj->get_type() == &Base::template get_type_id<Derived>;
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
