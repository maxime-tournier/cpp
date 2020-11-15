// -*- compile-command: "c++ -std=c++14 church.cpp -o church" -*-

#include <memory>


template<class T=void>
using fun_ptr = void(*)();


template<class A, class ... Args>
struct selector {

  template<class Derived>
  friend auto make_select(const Derived*, const A&) {
    return +[](fun_ptr<Args> ... ptrs) {
      fun_ptr<> res = nullptr;
      const int expand[] = {
          (std::is_same<A, Args>::value ? (res = ptrs, 0) : 0)...};
      return res;
    };
  };
  
};



template<class Visitor, class ... Args>
using visitor_result = std::common_type_t<std::result_of_t<Visitor(Args)>...>;


template<class ... Args>
class variant: selector<Args, Args...>... {
  using storage_type = std::aligned_union_t<0, Args...>;
  storage_type storage;

  fun_ptr<> (*select)(fun_ptr<Args>...);
public:
  template<class T>
  variant(const T& self): select(make_select(this, self)) {}

  template<class T>
  const T& get() const & {
    return *reinterpret_cast<const T*>(this);
  }

  template<class T>
  T&& get() && {
    return std::move(*reinterpret_cast<T*>(this));
  }
  
  template<class Self, class Visitor, class Ret=visitor_result<Visitor, Args...>>
  friend Ret visit(Self&& self, const Visitor& visitor) {
    using thunk_type = Ret (*)(Self&&, const Visitor& visitor);
    auto thunk = self.select(fun_ptr<Args>(+[](Self&& self,
                                                const Visitor& visitor) {
      
      return visitor(std::forward<Self>(self).template get<Args>());
    })...);

    return thunk_type(thunk)(std::forward<Self>(self), visitor);
  }

};


#include <iostream>

int main(int, char**) {
  variant<int, double> test = 1.0;

  visit(test, [](auto self) {
    std::clog << typeid(self).name() << std::endl;
  });
  
  return 0;
}



