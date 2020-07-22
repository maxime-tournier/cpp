#include <utility>



template<class T>
struct maybe: monad {
  T value;
  maybe(T value): value(std::move(value)) { }
  
  template<class Func>
  friend auto bind(maybe&& self, const Func& func) {
    return func(std::move(self.value));
  }

  template<class Func>
  friend auto bind(const maybe& self, const Func& func) {
    return func(self.value);
  }
  
};


int main(int, char**) {
  maybe<int> x(0);
  x >>= [](int x) { return maybe<double>(x); };
  
  return 0;
}
