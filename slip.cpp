// -*- compile-command: "c++ -o slip2 slip.cpp" -*-

// variant
#include <type_traits>
#include <utility>
#include <new>

// symbol
#include <set>

// refs
#include <memory>


#include <iostream>

namespace {
  template<std::size_t, class ... Args>
  struct helper;

  template<std::size_t I, class T>
  struct helper<I, T> {
    static constexpr std::size_t index(const T*) { return I; }

    static void construct(void* dest, const T& source) { new (dest) T(source); }
    static void construct(void* dest, T&& source) { new (dest) T{std::move(source)}; }
  };


  template<std::size_t I, class T, class ... Args>
  struct helper<I, T, Args...> : helper<I + 1, Args... >, helper<I, T> {
    using this_type = helper<I, T>;
    using other_types = helper<I + 1, Args... >;

    using this_type::index;
    using other_types::index;

    using this_type::construct;
    using other_types::construct;
  };
  
  struct destruct {
    template<class T>
    void operator()(const T& self) const { self.~T(); }
  };

  struct move {
    template<class T>
    void operator()(T& source, void* target) const {
      new (target) T(std::move(source));
    }
  };

  struct copy {
    template<class T>
    void operator()(T& source, void* target) const {
      new (target) T(source);
    }
  };


  template<class ... Func>
  struct overload : Func... {
    overload(const Func& ... func) : Func(func)... { }
  };
  
}
  
  
template<class ... Args>
class variant {
  using storage_type = typename std::aligned_union<0, Args...>::type;
  storage_type storage;

  using index_type = unsigned char;
  index_type index;

  using helper_type = helper<0, Args...>;

  template<class T, class Ret, class Self, class Func, class ... Params>
  static Ret thunk(Self&& self, Func&& func, Params&& ... params) {
    return std::forward<Func>(func)(std::forward<Self>(self).template get<T>(),
                                    std::forward<Params>(params)...);
  }

  static void derived(const variant&);

public:

  template<class T>
  const T& get() const { return reinterpret_cast<const T&>(storage); }

  template<class T>
  T& get() { return reinterpret_cast<T&>(storage); }

  
  template<class T, index_type index = helper_type::index( (T*) 0 )>
  variant(T&& value) : index(index) {
    helper_type::construct(&storage, std::forward<T>(value));
  }

  variant(variant&& other) : index(other.index) {
    other.visit<void>(move(), &storage);
  }
  
  variant(const variant& other) : index(other.index) {
    other.visit<void>(copy(), &storage);
  }

  ~variant() { visit<void>(destruct()); }
  
  template<class Ret = void, class Func, class ... Params>
  Ret visit(Func&& func, Params&& ... params) const {
    using ret_type = Ret;
    using self_type = const variant&;
    using thunk_type = ret_type (*) (self_type&&, Func&&, Params&&...);
    
    static const thunk_type table[] = {thunk<Args, ret_type, self_type, Func, Params...>...};
    
    return table[index](*this, std::forward<Func>(func), std::forward<Params>(params)...);
  }

  template<class Ret = void, class ... Func>
  Ret match(const Func& ... func) const {
    return visit<Ret>(overload<Func...>(func...));
  }
  
};

struct unit { };
using boolean = bool;
using integer = long;
using real = double;

class symbol {
  const char* name;
public:
  
  explicit symbol(const std::string& name) {
    static std::set<std::string> table;
    this->name = table.insert(name).first->c_str();
  }

  const char* get() const { return name; }
  bool operator<(const symbol& other) const { return name < other.name; }
  bool operator==(const symbol& other) const { return name == other.name; }  
};

template<class T>
using ref = std::shared_ptr<T>;

template<class T> struct cons;
template<class T> using list = ref<cons<T>>;

template<class T>
struct cons {
  T head;
  list<T> tail;

  cons(const T& head, const list<T>& tail) : head(head), tail(tail) { }
  
  friend list<T> operator>>=(const T& head, const list<T>& tail) {
    return std::make_shared<cons>(head, tail);
  }

  template<class Func>
  friend list< typename std::result_of<Func(T)>::type > map(const list<T>& self, const Func& func) {
    if(!self) return {};
    return func(self->head) >>= map(self->tail, func);
  }
  
};


struct sexpr : variant<real, integer, boolean, symbol, list<sexpr> > {
  using sexpr::variant::variant;
  using list = list<sexpr>;
};


struct lambda {
  const list<symbol> args;
  const sexpr body;
};

struct value : variant<real, integer, boolean, symbol, lambda, list<value> > {
  using value::variant::variant;
  using list = list<value>;
};

template<class Target>
struct cast {

  template<class T>
  Target operator()(const T& self) const { return self; }
  
};


// static value eval(const sexpr& expr) {
//   return match(expr,
//                [&](real x) -> value { return x; },
//                [&](integer x) -> value { return x; },
//                [&](boolean x) -> value { return x; },
//                [&](lambda x) -> value { return x; },
               
//                [&](symbol x) -> value { return x; },
//                [&](sexpr::list x) -> value {
//                  return 1.0;
//                });
// }



int main(int, char**) {

  const sexpr e = 2.0 >>= symbol("michel") >>= sexpr::list();

  e.match([](real d) { std::clog << "double" << std::endl; },
          [](integer d)  { std::clog << "long" << std::endl; },
          [](boolean d) { std::clog << "bool" << std::endl; },
          [](symbol s)  { std::clog << "symbol" << std::endl; },
          [](sexpr::list x){ std::clog << "list" << std::endl; }
          );
  
  return 0;
}
