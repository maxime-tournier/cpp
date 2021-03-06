#ifndef LIST_HPP
#define LIST_HPP

#include <memory>
// #include <stdexcept>

template<class T>
struct cons;

template<class T>
using list = std::shared_ptr<cons<T>>;

template<class T>
struct cons {
  const T head;
  const list<T> tail;
  cons(T head, list<T> tail): head(std::move(head)), tail(std::move(tail)) { }
  
  friend list<T> operator%=(T head, list<T> tail) {
    return std::make_shared<cons>(std::move(head), std::move(tail));
  }

  template<class X, class Func>
  friend X foldr(list<T> self, X x, const Func& func) {
    if(!self) {
      return x;
    } else {
      return func(self->head, foldr(self->tail, std::move(x), func));
    }
  }

  template<class Func>
  friend auto map(list<T> self, const Func& func) {
    using type = typename std::result_of<Func(T)>::type;
    return foldr(self, list<type>{}, [&](const T& head, list<type> tail) {
      return func(head) %= tail;
    });
  }

  template<class X, class Func>
  friend X foldl(X x, list<T> self, const Func& func) {
    if(!self) {
      return x;
    } else {
      return foldl(func(std::move(x), self->head), self->tail, func);
    }
  }

  
  struct iterator {
    list<T> data;
    iterator& operator++() { data = data->tail; return *this; }
    bool operator!=(iterator other) { return data != other.data; }
    auto& operator*() const { return data->head; }
  };
  
  friend iterator begin(list<T> self) { return {self}; }
  friend iterator end(list<T> self) { return {nullptr}; }

  friend list<T> concat(list<T> lhs, list<T> rhs) {
    if(lhs) {
      return lhs->head %= concat(lhs->tail, std::move(rhs));
    } else {
      return rhs;
    }
  }


  friend std::size_t size(list<T> self) {
    return foldr(self, 0, [](auto lhs, auto rhs) { return 1 + rhs; });
  }
  
  // // quick
  // template<class What>
  // friend list<T> operator||(list<T> self, What what) {
  //   if(self) return self;
  //   throw std::runtime_error(what);
  // }
};


template<class Iterator>
static list<typename Iterator::value_type> make_list(Iterator first, Iterator last) {
  if(first == last) {
    return {};
  } else {  
    auto head = std::move(*first++);
    return head %= make_list(first, last);
  }
}

template<class LHS, class RHS, class Func>
static auto zip(list<LHS> lhs, list<RHS> rhs, const Func& func)
    -> list<typename std::result_of<Func(LHS, RHS)>::type> {
  if(lhs && rhs) {
    return func(lhs->head, rhs->head) %= zip(lhs->tail, rhs->tail, func);
  }

  return {};
};


#endif
