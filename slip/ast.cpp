#include "ast.hpp"

#include "syntax.hpp"

namespace slip {

  namespace ast {


    static expr check_expr(const sexpr& e);
    

    using special_type = expr (const sexpr::list& e);

    static special_type check_lambda,
      check_application,
      check_definition,
      check_condition;
    
    static sequence::binding check_sequence_binding(const sexpr::list&);
    
    
    static const std::map< symbol, special_type* > special = {
      {kw::lambda, check_lambda},
      {kw::def, check_definition},
      {kw::cond, check_condition},                  
    };
    

    
    static expr check_lambda(const sexpr::list& args) {
      struct fail { };

      try {
        if(size(args) != 2 || !args->head.is<sexpr::list>() ) throw fail();
        
        const list<symbol> vars = map(args->head.get<sexpr::list>(), [&](const sexpr& e) {
            if(!e.is<symbol>()) throw fail();
            return e.get<symbol>();
          });
        return make_ref<lambda>(vars, check_expr(args->tail->head));
        
      } catch( fail& ) {
        throw syntax_error("(lambda (`symbol`...) `expr`)");
      }
      
    }
    

    static expr check_application(const sexpr::list& self) {
      if(!self) {
        throw syntax_error("empty list in application");
      }

      return make_ref<application>( check_expr(self->head), map(self->tail, [](const sexpr& e) {
            return check_expr(e);
          }));
    }

    
    static expr check_definition(const sexpr::list& items) {
      struct fail { };
      
      try {
        
        if( size(items) != 2 ) throw fail();
        if( !items->head.is<symbol>() ) throw fail();

        return make_ref<definition>(items->head.get<symbol>(), check_expr(items->tail->head));
      } catch( fail& ) {
        throw syntax_error("(def `symbol` `expr`)");
      }
    }



    static expr check_condition(const sexpr::list& items) {
      struct fail {};
      
      try{

        return make_ref<condition>(map(items, [](const sexpr& e) {
              if(!e.is<sexpr::list>()) throw fail();
              const sexpr::list& lst = e.get<sexpr::list>();

              if(size(lst) != 2) throw fail();

              return condition::branch(check_expr(lst->head),
                                       check_expr(lst->tail->head));
            }));
        
      } catch( fail ) {
        throw syntax_error("(cond (`expr` `expr`)...)");
      }
      
    }

    
    
    
    
    struct expr_visitor {

      template<class T>
      expr operator()(const T& self) const {
        return literal<T>{self};
      }
      
      expr operator()(const symbol& self) const {
        return variable{self};
      }


      expr operator()(const ref<string>& self) const {
        return literal<string>{*self};
      }
      
      
      expr operator()(const sexpr::list& self) const {
        if(self && self->head.is<symbol>() ) {
          auto it = special.find(self->head.get<symbol>());
          if(it != special.end()) {
            return it->second(self->tail);
          }
        }

        return check_application(self);
      }      
      
    };
    

    static expr check_expr(const sexpr& e) {
      return e.map<expr>(expr_visitor());
    }
    

    struct sequence_item_visitor : expr_visitor {

      template<class T>
      sequence::item operator()(const T& self) const {
        return expr_visitor::operator()(self);
      }
      
      sequence::item operator()(const sexpr::list& self) const {
        if(self && self->head.is<symbol>() && self->head.get<symbol>() == kw::var) {
          return check_sequence_binding(self->tail);
        }

        return expr_visitor::operator()(self);
      }
      
    };



    static sequence::binding check_sequence_binding(const sexpr::list& items) {
      struct fail { };
      
      try {
        
        if( size(items) != 2 ) throw fail();
        if( !items->head.is<symbol>() ) throw fail();

        return sequence::binding(items->head.get<symbol>(), check_expr(items->tail->head));
      } catch( fail& ) {
        throw syntax_error("(var `symbol` `expr`)");
      }
    }
    
    sequence::item check_sequence_item(const sexpr& e) {
      return e.map<sequence::item>(sequence_item_visitor());
    }


    struct toplevel_visitor : sequence_item_visitor {

      template<class T>
      toplevel operator()(const T& self) const {
        return sequence_item_visitor::operator()(self);
      }
      
    };

    toplevel check(const sexpr& e) {
      return e.map<toplevel>(toplevel_visitor());
    }

    
  }
  
}
