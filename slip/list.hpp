#ifndef LISP_LIST_HPP
#define LISP_LIST_HPP

#include "../ref.hpp"
#include "error.hpp"

#include <ostream>


namespace slip {

  template<class Head> struct cell;

  template<class Head>
  using list = ref< cell<Head> >;

  
  // lists
  template<class Head>
  struct cell {

    using head_type = Head;
    head_type head;

    using tail_type = list<Head>;
    tail_type tail;

    cell(const head_type& head,
         const tail_type& tail)
      : head(head),
        tail(tail) { 

    }
    
    struct iterator {
      cell* ptr;

      inline head_type& operator*() const { return ptr->head; }
      inline bool operator!=(iterator other) const { return ptr != other.ptr; }
      inline iterator& operator++() { ptr = ptr->tail.get(); return *this; }
      inline iterator operator++(int) {
        iterator res = *this;
        ptr = ptr->tail.get();
        return res;
      }
    };


    
    
  };

  struct empty_list : error {
    empty_list() : error("empty list") { } 
  };

  

  template<class Head>
  static inline typename cell<Head>::iterator begin(const list<Head>& self) { return { self.get() }; }

  template<class Head>
  static inline typename cell<Head>::iterator end(const list<Head>& self) { return { nullptr }; }


  template<class Head>
  static std::size_t size(const list<Head>& self) {
    if(!self) return 0;
    return 1 + size(self->tail);
  }
  
  template<class Head, class F>
  static inline list< typename std::result_of< F(Head) >::type > map(const list<Head>& self, const F& f) {
    if(!self) return {};
    const auto head = f(self->head);
    return head >>= map(self->tail, f);
  }


  template<class Head, class F, class Init = Head>
  static inline Init foldr(const Init& init, const list<Head>& self, const F& f) {
    if(!self) return init;
    return f(self->head, foldr(init, self->tail, f));
  }


  template<class Head, class F, class Init = Head>
  static inline Init foldl(const Init& init, const list<Head>& self, const F& f) {
    if(!self) return init;
    return foldl( f(init, self->head), self->tail, f);
  }
  

  template<class Head>
  static inline list<Head> concat(const list<Head>& lhs, const list<Head>& rhs) {
    return foldr(rhs, lhs, [](const Head& head, const list<Head>& rest) {
        return head >>= rest;
      });
  }

  // TODO finish
  template<class Ret, class Head, class F, std::size_t ... I>
  static inline Ret unpack(const list<Head>& self, const F& f) {
    auto it = begin(self);
    
    const Head* expand [] = { (I, *it++)... };
    
    return f( expand[I]... );
  };
  
  template<class Head>
  static inline list< list<Head> > split(std::size_t at, const list<Head>& arg) {
    if(at == 0) return list<Head>() >>= arg >>= list<list<Head>>();
    if(!arg) throw empty_list();

    const list<list<Head>>& sub = split(at - 1, arg->tail);
    return (arg->head >>= sub->head) >>= (sub->tail->head) >>= list<list<Head>>();;
  }

  
  template<class Head>
  const Head& get( const list<Head>& self, std::size_t i ) {
    if(i == 0) return self->head;
    return get(self->tail, i - 1);
  }
  
  
  template<class H, class Iterator>
  static list<H> make_list(Iterator first, Iterator last) {
    list<H> res;
    list<H>* curr = &res;
    
    for(Iterator it = first; it != last; ++it) {
      *curr = *it >>= list<H>();
      curr = &(*curr)->tail;
    }
    
    return res;
  }
 


  
  // static inline const list& unpack(const list& from) { return from; }
  
  // template<class H, class ... Args>
  // static inline const list& unpack(const list& from, H* to, Args*... args) {
  //   if(!from) throw empty_list();
  //   *to = from->head.cast<H>();
  //   return unpack(from->tail, args...);
  // }

  template<class H>
  std::ostream& operator<<(std::ostream& out, const list<H>& self) {
    bool first = true;
    for(const H& x : self) {
      if(first) first = false;
      else out << ' ';
      out << x;
    }
    return out;
  }

  
  template<class H>
  static inline typename cell<H>::head_type& head(const list<H>& self) {
    if(!self) throw empty_list();
    return self->head;
  }

  template<class H>
  static inline const list<H>& tail(const list<H>& x) {
    if(!x) throw empty_list();
    return x->tail;
  }
  

  template<class H, class T>
  static inline list<H> operator>>=(const T& head, const list<H>& tail) {
    return make_ref<cell<H>>(head, tail);
  }
  
  
  


}


#endif
