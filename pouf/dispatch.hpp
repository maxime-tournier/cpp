#ifndef DISPATCH_HPP
#define DISPATCH_HPP


// apply itself on the same variant after calling ->cast()
template<class Derived>
struct dispatch {

  const Derived& derived() const { 
	return static_cast<const Derived&>(*this);
  }

  // note: we don't want perfect forwarding here since it may preempt
  // derived classes operator()
  template<class T, class ... Args>
  void operator()(T* self, const Args& ... args) {
    self->cast.apply( derived(), args...);
  }
  
};






#endif
