#include <string>
#include <functional>

#include <iostream>
#include <sstream>

namespace v1 {

  auto lit(int i) { 
    return [=](auto lit, auto add) {
      return lit(i);
    };
  }

  template<class A, class B>
  auto add(A a, B b) {
    return [=](auto lit, auto add) { 
      return add(a(lit, add), b(lit, add));
    };
  }

}


namespace v2 {

  // auto lit(int i) { 
  //   return [=](auto self) {
  //     return self.lit(i);
  //   };
  // }

  // template<class A, class B>
  // auto add(auto self) {
  //   return [=](auto self) { 
  //     return self.add(a(self , b(lit, add));
  //   };
  // }

}





template<class T>
struct ast {
  virtual T lit(int) const = 0;
  virtual T add(T, T) const = 0;
};


template<class T>
auto lit(int i) {
  return [=](const ast<T>& self) {
    return self.lit(i);
  };
}

template<class T>
auto add(T lhs, T rhs) {
  return [=](const ast<T>& self) {
    return self.add(lhs, rhs);
  };
}


struct printer: ast<std::string> {
  std::string lit(int i) const override { return std::to_string(i); }
  std::string add(std::string lhs, std::string rhs) const override { 
    return lhs + " + " + rhs;
  }
};

// reader monad
static const auto pure = [](auto value) {
  return [value](const auto&) { return value; };
};

static const auto bind = [](auto ma, auto f) {
  return [=](const auto& env) {
    const auto a = ma(env);
    const auto mb = f(a);
    return mb(env);
  };
};

template<class MA, class F>
static auto operator>>=(MA ma, F f) {
  return bind(ma, f);
}






template<class T>
std::function<T(const ast<T>&)> parse(std::istream& in) {
  const auto pos = in.tellg();

  int i;
  if(in >> i) {
    return lit<T>(i);
  }
  in.clear();
  in.seekg(pos);

  std::string s;
  if(in >> s) {
    if(s == "+") {
      return parse<T>(in) >>= [&](T lhs) {
        return parse<T>(in) >>= [&](T rhs) {
          return add(lhs, rhs);
        };
      };
    }
  }

  throw std::runtime_error("parse error");
}



int main(int argc, char** argv) {
  if(argc <= 1) return -1;
  
  std::stringstream ss(argv[1]);
  auto expr = parse<std::string>(ss)(printer());

  std::cout << expr << std::endl;

  {
    using namespace v1;
    
    auto x = add(lit(1), lit(2));

  }

  return 0;
}
