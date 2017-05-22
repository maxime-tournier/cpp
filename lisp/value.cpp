#include "value.hpp"
#include "eval.hpp"

namespace lisp {

  lambda::lambda(const ref<context>& ctx,
                 args_type&& args,
                 const value& body)
    : ctx(ctx),
      args(std::move(args)),
      body(body) {

  }

  symbol::table_type& symbol::table() {
    static table_type instance;
    return instance;
  }
  

  struct value::ostream {

    void operator()(const list& self, std::ostream& out) const {
      bool first = true;
      out << '(';
      for(const value& x : self) {
        if(first) first = false;
        else out << ' ';
        out << x;
      }
      out << ')';
    }

    void operator()(const symbol& self, std::ostream& out) const {
      out << self.name();
    }

    void operator()(const ref<string>& self, std::ostream& out) const {
      out << '"' << *self << '"';
    }

    void operator()(const builtin& self, std::ostream& out) const {
      out << "#<builtin>";
    }

    void operator()(const ref<lambda>& self, std::ostream& out) const {
      out << "#<lambda>";
    }
    
      
    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << self;
    }
    
  };

  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.apply( value::ostream(), out );
    return out;
  }
  

}
