#include <utility>

template<class A>
struct tag {
  friend constexpr int adl_flag(tag);
};

template <class A>
struct writer {
  friend constexpr int adl_flag(tag<A>) {
    return 0;
  }
};




template<class A, int = adl_flag( tag<A>() )>
constexpr bool can_dereference(int) {
  return false;
}

template<class A>
constexpr bool can_dereference (...) {
  return true;
}


template<class U, class A>
class safe { 
  U value;

  // triggers instantiation of tag<A> and declares adl_flag so that
  // writer will be able to define. otherwise adl_flag will not be
  // found through adl since unrelated to writer.
  static const int trigger = sizeof(tag<A>);
  
public:

  template<class T = A, bool moveable = can_dereference<T>(0) >
  U& operator*() {
	static_assert(moveable, "value has been moved");
	return value;
  }
  
  template<class B, int = sizeof(writer<B>)>
  safe& operator=(safe<U, B>&& ) {
	return *this;
  }

  safe(U value = {}) : value(value) {
  }
  
};

int main() {
  struct id_a;
  struct id_b;
  
  // constexpr int a = f<test>();
  // constexpr int b = f<test>();
  
  // static_assert(a != b, "fail");
  safe<int, id_a> a;
  safe<int, id_b> b;
  
  // *b;

  a = std::move(b);
  // is_flag_usable<id_b>(0);
  *b;
  *a;
  return 0;
}


