#include "types.hpp"

#include "sexpr.hpp"
#include "ast.hpp"

#include <algorithm>
#include <sstream>

namespace slip {

  namespace types {

    // pretty-printing types
    struct pretty_printer {

      struct var {
        std::size_t index;
        bool quantified;

        friend  std::ostream& operator<<(std::ostream& out, const var& self) {
          if( self.quantified ) out << "'";
          else out << "!";
          return out << char('a' + self.index);
        }
        
      };

      using ostream_map = std::map< ref<variable>, var >;

      struct ostream_visitor;

      
      ostream_map osm;
      std::ostream& out;
      pretty_printer(std::ostream& out) : out(out) { }


      template<class T>
      pretty_printer& operator<<(const T& self) {
        out << self;
        return *this;
      }

      pretty_printer& operator<<(const type& self);

      pretty_printer& operator<<( std::ostream& (*f)(std::ostream& )) {
        out << f;
        return *this;
      }
      
    };

    struct pretty_printer::ostream_visitor {

      void operator()(const constant& self, std::ostream& out, 
                      ostream_map& osm) const {
        out << self.name;
      }

      void operator()(const ref<variable>& self, std::ostream& out, 
                      ostream_map& osm) const {
        auto err = osm.emplace( std::make_pair(self, var{osm.size(), false} ));
        out << err.first->second;
      }

      void operator()(const ref<application>& self, std::ostream& out,
                      ostream_map& osm) const {
        // TODO parentheses

        if(self->func == func_ctor) {
          self->arg.apply(ostream_visitor(), out, osm);
          out << ' ';
          self->func.apply(ostream_visitor(), out, osm);
        } else {
          self->func.apply(ostream_visitor(), out, osm);
          out << ' ';
          self->arg.apply(ostream_visitor(), out, osm);
        }
         
      }

      
    };


    pretty_printer& pretty_printer::operator<<(const type& self) {
      self.apply( ostream_visitor(), out, osm );        
      return *this;
    }





    // kinds
    constructor operator>>=(const kind& lhs, const kind& rhs) {
      return constructor(lhs, rhs);
    }


    bool constructor::operator==(const constructor& other) const {
      return *from == *other.from && *to == *other.to;
    }

    bool constructor::operator<(const constructor& other) const {
      return *from < *other.from || (*from == *other.from && *to < *other.to);
    }

    
    
    const type func_ctor = constant("->", terms() >>= terms() >>= terms() );
    const type io_ctor = constant("io", terms() >>= terms() );
    const type list_ctor = constant("list", terms() >>= terms() );        

    const type record_ctor = constant("record", rows() >>= terms());    
    // TODO variant ctor


    
    type type::operator()(const type& arg) const {
      return make_ref<application>(*this, arg);
    }
    
    type operator>>=(const type& lhs, const type& rhs) {
      return (func_ctor(lhs))(rhs);

    }
    

    bool constant::operator==(const constant& other) const {
      // std::clog << kind << " vs. " << other.kind << std::endl;
      return name == other.name && kind == other.kind;
    }

    bool constant::operator<(const constant& other) const {
      return name < other.name || (name == other.name && kind < other.kind);
    }


    
    struct type_kind_visitor {
      
      kind operator()(const constant& self) const { return self.kind; }
      kind operator()(const ref<variable>& self) const { return self->kind; }    
      kind operator()(const ref<application>& self) const  {
        return *self->func.kind().get<constructor >().to;
      }  
    
    };
  

    struct kind type::kind() const {
      return map<struct kind>(type_kind_visitor());
    }


    struct kind_ostream {

      void operator()(const terms&, std::ostream& out) const {
        out << '*';
      }

      
      void operator()(const rows&, std::ostream& out) const {
        out << "{}";
      }
      

      void operator()(const constructor& self, std::ostream& out) const {
        // TODO parentheses
        out << *self.from << " -> " << *self.to;
      }
      
      
    };
    
    std::ostream& operator<<(std::ostream& out, const kind& self) {
      self.apply(kind_ostream(), out);
      return out;      
    }


    // term types
    const constant unit_type("unit"),
                            boolean_type("boolean"),
                            integer_type("integer"),
                            real_type("real"),
                            string_type("string"),
                            symbol_type("symbol");
    

    // row types
    const type empty_row_type = constant("{}", rows());


    static inline type row_extension(symbol label) {
      return constant(label.name(), terms() >>= rows() >>= rows());
    }
    
  
    template<> constant traits< slip::unit >::type() { return unit_type; }
    template<> constant traits< slip::boolean >::type() { return boolean_type; }      
    template<> constant traits< slip::integer >::type() { return integer_type; }
    template<> constant traits< slip::real >::type() { return real_type; }
    template<> constant traits< slip::symbol >::type() { return symbol_type; }

    // mew
    template<> constant traits< ref<slip::string> >::type() { return string_type; }
    

    state::state(ref<env_type> env, ref<uf_type> uf) 
      : env( env ),
        uf( uf ) {
      
    }

    
    static type infer(state& self, const ast::expr& node);
    


    // unification
    struct unification_error {
      unification_error(type lhs, type rhs) : lhs(lhs), rhs(rhs) { }
      const type lhs, rhs;
    };

    
    struct occurs_error // : unification_error
    {
      occurs_error(const ref<variable>& var, const struct type& type)
        : var(var), type(type) {

      }
      
      ref<variable> var;
      struct type type;
    };



    template<class UF>
    struct occurs_check {

      bool operator()(const constant& self, const ref<variable>& var, UF& uf) const {
        return false;
      }

      bool operator()(const ref<variable>& self, const ref<variable>& var, UF& uf) const {
        // TODO not sure if this should be done here
        
        if(self->depth > var->depth) {
          // we are trying to unify var with a type type containing self,
          // but var has smaller depth: we need to "raise" self to var's depth
          // so that self generalizes just like var

          // note: we keep self->kind
          const ref<variable> raised = make_ref<variable>(self->kind, var->depth);
          assert( type(self).kind() == uf.find(self).kind());
          
          assert(uf.find(self).kind() == type(raised).kind());
          uf.link(uf.find(self), raised);
        }
        
        std::clog << self.get() << " " << var.get() << std::endl;
        return self == var;
      }


      bool operator()(const ref<application>& self, const ref<variable>& var, UF& uf) const {

        return
          uf.find(self->arg).template map<bool>(occurs_check(), var, uf) ||
          uf.find(self->func).template map<bool>(occurs_check(), var, uf);
      }
      
    };

    

    template<class UF>
    struct unify_visitor {

      const bool try_reverse;
      
      unify_visitor(bool try_reverse = true) 
        : try_reverse(try_reverse) { }
      
      template<class T>
      void operator()(const T& self, const type& rhs, UF& uf) const {
        
        // double dispatch
        if( try_reverse ) {
          return rhs.apply( unify_visitor(false), self, uf);
        } else {
          throw unification_error(self, rhs);
        }
      }


      void operator()(const ref<variable>& self, const type& rhs, UF& uf) const {
        pretty_printer(std::clog) << "unifying: " << type(self) << " with: " << rhs << std::endl;
        
        assert( uf.find(self) == self );
        assert( uf.find(rhs) == rhs );        
        
        if( type(self) != rhs && rhs.map<bool>(occurs_check<UF>(), self, uf)) {
          throw occurs_error{self, rhs.get< ref<application> >()};
        }

        if( self->kind != rhs.kind() ) {
          std::stringstream ss;

          pretty_printer(ss) << "when unifying: " << type(self) << " :: " << self->kind 
                             << "with: " << rhs << " :: " << rhs.kind();
          
          throw kind_error(ss.str());
        }
        
        // debug( std::clog << "linking", type(self), rhs ) << std::endl;
        uf.link(self, rhs);
        
      }


      void operator()(const constant& lhs, const constant& rhs, UF& uf) const {
        pretty_printer(std::clog) << "unifying: " << type(lhs) << " with: " << type(rhs) << std::endl;
        
        if( !(lhs == rhs) ) {
          throw unification_error(lhs, rhs);
        }
      }


      type tail(const type& row) const {
        assert( row.is<ref<application> >() );

        const ref<application>& app = row.get< ref<application> >();

        assert( app->arg.kind() == rows() );
        return app->arg;
      }
      

      void operator()(const ref<application>& lhs, const ref<application>& rhs, UF& uf) const {
        pretty_printer(std::clog) << "unifying: " << type(lhs) << " with: " << type(rhs) << std::endl;        

        try{
          UF tmp = uf;
          tmp.find(lhs->arg).apply( unify_visitor(), tmp.find(rhs->arg), tmp);
          tmp.find(lhs->func).apply( unify_visitor(), tmp.find(rhs->func), tmp);
          uf = std::move(tmp);
        } catch( unification_error& e ) {

          // if we're unifying row types and error is on labels
          if( type(lhs).kind() == rows() && e.lhs.kind() != terms() ) {

            try {
              // try lhs tail with rhs
              pretty_printer(std::clog) << "trying: " << tail(lhs) << " with: " << type(rhs) << std::endl;
              uf.find( tail(lhs) ).apply(unify_visitor(), rhs, uf);
              
              std::clog << "success !" << std::endl;
              return;
              
            } catch( unification_error& e ) {
              pretty_printer(std::clog) << "nope, trying: " << type(lhs) << " with: " << tail(rhs) << std::endl;
              type(lhs).apply(unify_visitor(), uf.find( tail(rhs)), uf);            

              // success!
              std::clog << "success !" << std::endl;
              return;
            }

            
          }
          
          throw;
        }


        
      }

      
    };


    void state::unify(const type& lhs, const type& rhs) {
      pretty_printer(std::clog) << "unifying: " << lhs << " with: " << rhs << std::endl;              
      uf->find(lhs).apply( unify_visitor<uf_type>(), uf->find(rhs), *uf);
    }
    
    


    
    // expression inference
    struct expr_visitor {

      
      template<class T>
      type operator()(const ast::literal<T>& self, state& tc) const {
        return traits<T>::type();
      }

      
      type operator()(const symbol& self, state& tc) const {
        const scheme& p = tc.find(self);
        return tc.instantiate(p);
      }

      
      type operator()(const ref<ast::lambda>& self, state& tc) const {

        state sub = tc.scope();

        // create/define arg types
        const list< ref<variable> > args = map(self->args, [&](const symbol& s) {
            const ref<variable> var = tc.fresh();
            // note: var stays monomorphic after generalization
            sub.def(s, sub.generalize(var) );
            return var;
          });
        
        // infer body type in subcontext
        const type body_type = infer(sub, self->body);

        // return complete application type
        return foldr(body_type, args, [](const ref<variable>& lhs, const type& rhs) {
            return lhs >>= rhs;
          });
        
      }



      type operator()(const ref<ast::application>& self, state& tc) const {

        const type func = infer(tc, self->func);

        // infer arg types
        const list<type> args = map(self->args, [&](const ast::expr& e) {
            return infer(tc, e);
          });

        // construct function type
        const type result = tc.fresh();
        
        const type sig = foldr(result, args, [&](const type& lhs, const type& rhs) {
            return lhs >>= rhs;
          });

        try{
          tc.unify(func, sig);
        } catch( occurs_error ) {
          throw type_error("occurs check");
        }

        return result;
      }


      type operator()(const ast::condition& self, state& tc) const {
        const type result = tc.fresh();

        for(const ast::branch& b : self.branches() ) {

          const type test = infer(tc, b.test);
          tc.unify(boolean_type, test);
          
          const type value = infer(tc, b.value);
          tc.unify(value, result);
        };

        return result;
      }


      // let-binding
      type operator()(const ref<ast::definition>& self, state& tc) const {

        const type value = tc.fresh();
        
        state sub = tc.scope();

        // note: value is bound in sub-context (monomorphic)
        sub.def(self->id, sub.generalize(value));

        tc.unify(value, infer(sub, self->value));

        tc.def(self->id, tc.generalize(value));
        
        return io_ctor( unit_type );
        
      }


      // monadic binding
      type operator()(const ref<ast::binding>& self, state& tc) const {

        const type value = tc.fresh();

        // note: value is bound in sub-context (monomorphic)
        tc = tc.scope();
        
        tc.def(self->id, tc.generalize(value));

        tc.unify(io_ctor(value), infer(tc, self->value));

        tc.find(self->id);
        
        return io_ctor( unit_type );
        
      }
      

      type operator()(const ast::sequence& self, state& tc) const {
        type res = io_ctor( unit_type );

        for(const ast::expr& e : self.items() ) {

          res = io_ctor( tc.fresh() );
          tc.unify( res, infer(tc, e));          
        }
          
        return res;
      }


      type operator()(const ast::selection& self, state& tc) const {

        const type a = tc.fresh(), r = tc.fresh(rows());
        
        return record_ctor( row_extension(self.label)(a)(r) ) >>= a;
      }
      


      type operator()(const ast::record& self, state& tc) const {

        return record_ctor( foldr(empty_row_type, self, [&](const ast::row& lhs, const type& rhs) {
              return row_extension(lhs.label)(infer(tc, lhs.value))(rhs);
            }));
        
      }
      
      type operator()(const ast::expr& self, state& tc) const {
        std::stringstream ss;
        ss << "type inference unimplemented for " << self;
        throw error(ss.str());
      }
      
    };


    static type infer(state& self, const ast::expr& node) {
      try{
        return node.map<type>(expr_visitor(), self);
      } catch( unification_error& e )  {
        std::stringstream ss;

        pretty_printer pp(ss);
        pp << "cannot unify: " << e.lhs << " with: " << e.rhs;
        
        throw type_error(ss.str());
      }
    }



    
    // finding nice representants
    struct nice {

      template<class UF>
      type operator()(const constant& self, UF& uf) const {
        return self;
      }


      template<class UF>
      type operator()(const ref<variable>& self, UF& uf) const {

        const type res = uf->find(self);
        
        if(res == type(self)) {
          // debug(std::clog << "nice: ", type(self), res) << std::endl;          
          return res;
        }
        
        return res.map<type>(nice(), uf);
      }


      template<class UF>
      type operator()(const ref<application>& self, UF& uf) const {
        return map(self, [&](const type& c) {
            return c.map<type>(nice(), uf);
          });
      }
      
    };




    // instantiation
    struct instantiate_visitor {
      using map_type = std::map< ref<variable>, ref<variable> >;
      
      type operator()(const constant& self, const map_type& m) const {
        return self;
      }

      type operator()(const ref<variable>& self, const map_type& m) const {
        auto it = m.find(self);
        if(it == m.end()) return self;
        return it->second;
      }

      type operator()(const ref<application>& self, const map_type& m) const {
        return map(self, [&](const type& c) {
            return c.map<type>(instantiate_visitor(), m);
          });
      }
      
    };
    


    type state::instantiate(const scheme& poly) const {
      instantiate_visitor::map_type map;

      // associate each bound variable to a fresh one
      auto out = std::inserter(map, map.begin());
      std::transform(poly.forall.begin(), poly.forall.end(), out, [&](const ref<variable>& v) {
          return std::make_pair(v, fresh(v->kind));
        });
      
      return poly.body.map<type>(instantiate_visitor(), map);
    }




    // env lookup/def
    struct unbound_variable : type_error {
      unbound_variable(symbol id) : type_error("unbound variable " + id.name()) { }
    };

    
    const scheme& state::find(symbol id) const {

      if(scheme* p = env->find(id)) {
        return *p;
      }

      throw unbound_variable(id);
    }


    state& state::def(symbol id, const scheme& p) {
      auto res = env->locals.emplace( std::make_pair(id, p) );
      if(!res.second) throw error("redefinition of " + id.name());
      return *this;
    }
    

    ref<variable> state::fresh(kind k) const {
      return make_ref<variable>(k, env->depth);
    }


    state state::scope() const {
      // TODO should we use nested union-find too?
      ref<env_type> sub = make_ref<env_type>(env);

      return state( sub, uf);
    }
    


    // variables
    struct vars_visitor {

      using result_type = std::set< ref<variable> >;
    
      void operator()(const constant&, result_type& res) const { }
      
      void operator()(const ref<variable>& self, result_type& res) const {
        res.insert(self);
      }

      void operator()(const ref<application>& self, result_type& res) const {
        self->func.apply( vars_visitor(), res );
        self->arg.apply( vars_visitor(), res );      
      }
    
    };


    static vars_visitor::result_type vars(const type& self) {
      vars_visitor::result_type res;
      self.apply(vars_visitor(), res);
      return res;
    }
    
    

    // generalization
    scheme state::generalize(const type& mono) const {
      // debug(std::clog << "gen: ", mono) << std::endl;
      scheme res(mono.map<type>(nice(), uf));

      const auto all = vars(res.body);
    
      // copy free variables (i.e. instantiated at a larger depth)
      auto out = std::back_inserter(res.forall);

      const std::size_t depth = this->env->depth;
      std::copy_if(all.begin(), all.end(), out, [depth](const ref<variable>& v) {
          return v->depth >= depth;
        });

      return res;
    }
  
  
    scheme infer(state& self, const ast::toplevel& node) {
      const type res = infer(self, node.get<ast::expr>());
      return self.generalize(res);
    }


    
    


    
    scheme::scheme(const type& body): body(body) {
      if(body.kind() != terms()) {
        std::stringstream ss;
        ss << "expected: " << kind( terms()) << ", got: " << body.kind();
        throw kind_error(ss.str());
      }
    }
    

    std::ostream& operator<<(std::ostream& out, const scheme& self) {
      pretty_printer pp(out);
      
      for(const ref<variable>& var : self.forall) {
        pp.osm.emplace( std::make_pair(var, pretty_printer::var{pp.osm.size(), true} ) );
      }
      
      pp << self.body;
      return out;
    }





  }
  
}
