#include "types.hpp"

#include "sexpr.hpp"
#include "ast.hpp"

#include <algorithm>
#include <sstream>

namespace slip {

  namespace types {

    // env lookup/def
    struct unbound_variable : type_error {
      unbound_variable(symbol id) : type_error("unbound variable \"" + id.str() + "\"") { }
    };

    
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
      std::ostream& ostream;
      bool flag = true;
      
      std::size_t depth = 0;
      
      pretty_printer(std::ostream& ostream) : ostream(ostream) { }

      struct indent_type {
        pretty_printer* owner;
        indent_type(pretty_printer* owner) : owner(owner) {
          ++owner->depth;
        }

        ~indent_type() {
          --owner->depth;
        }
      };

      indent_type indent() { return {this}; }
      
      std::ostream& out() {
        if(flag) {
          for(std::size_t i = 0; i < depth; ++i) {
            ostream << "  ";
          }
          
          flag = false;
        }

        return ostream;
      }
      
      template<class T>
      pretty_printer& operator<<(const T& self) {
        out() << self;
        return *this;
      }

      pretty_printer& operator<<(const type& self);

      pretty_printer& operator<<( std::ostream& (*f)(std::ostream& )) {
        out() << f;

        using type = decltype(f);
        
        if(f == (type)std::endl) {
          flag = true;
        }
        return *this;
      }
      
    };

    struct pretty_printer::ostream_visitor {

      bool parens;
      
      ostream_visitor(bool parens = false)
        : parens(parens) {

      }
      
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

        // i have no idea what i'm doing lol

        if(self->func == func_ctor) {
          self->arg.apply(ostream_visitor(true), out, osm);
          out << ' ';
          self->func.apply(ostream_visitor(true), out, osm);
        } else {
          if(parens) out << '(';
          // const bool sub = self->func.is< ref<application> >();
          
          self->func.apply(ostream_visitor(true), out, osm);
          out << ' ';
          self->arg.apply(ostream_visitor(false), out, osm);
          if(parens) out << ')';
        
        }

      }

      
    };


    pretty_printer& pretty_printer::operator<<(const type& self) {
      self.apply( ostream_visitor(), out(), osm );        
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
    
    
    const type func_ctor = make_constant("->", terms() >>= terms() >>= terms() );
    const type list_ctor = make_constant("list", terms() >>= terms() );
    const type io_ctor = make_constant("io", terms() >>= terms() );
    

    // row extension constructor
    static constant row_extension_ctor(symbol label) {
      return constant(label, terms() >>= rows() >>= rows());
    }

    const type record_ctor = make_constant("record", rows() >>= terms());
    

    type type::operator()(const type& arg) const {
      return make_ref<application>(*this, arg);
    }
    
    type operator>>=(const type& lhs, const type& rhs) {
      return (func_ctor(lhs))(rhs);

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
        out << "row";
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


    application::application(type func, type arg)
      : func(func),
        arg(arg) {
      
      if(!func.kind().is< constructor>()) {
        throw kind_error("type constructor expected");
      }
      
      if(*func.kind().get< constructor >().from != arg.kind() ) {
        std::stringstream ss;
        ss << "expected: " << *func.kind().get< constructor >().from
           << ", got: " << arg.kind();
           
        throw kind_error(ss.str());
      }
    }



    


    
    // term types
    const type unit_type = make_constant("unit"),
              boolean_type = make_constant("boolean"),
              integer_type = make_constant("integer"),
              real_type = make_constant("real"),
              string_type = make_constant("string"),
              symbol_type = make_constant("symbol");
    

    

    
  
    template<> type traits< slip::unit >::type() { return unit_type; }
    template<> type traits< slip::boolean >::type() { return boolean_type; }      
    template<> type traits< slip::integer >::type() { return integer_type; }
    template<> type traits< slip::real >::type() { return real_type; }
    template<> type traits< slip::symbol >::type() { return symbol_type; }

    // mew
    template<> type traits< ref<slip::string> >::type() { return string_type; }
    

    state::state(ref<env_type> env, ref<uf_type> uf, ref<ctor_type> ctor) 
      : env( env ),
        uf( uf ),
        ctor(ctor) {
      
    }


    
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

      pretty_printer& pp;
      const bool try_reverse;
      
      unify_visitor(pretty_printer& pp, bool try_reverse = true) 
        : pp(pp),
          try_reverse(try_reverse) { }
      
      template<class T>
      void operator()(const T& self, const type& rhs, UF& uf) const {
        
        // double dispatch
        if( try_reverse ) {
          return rhs.apply( unify_visitor(pp, false), self, uf);
        } else {
          throw unification_error(self, rhs);
        }
      }


      void operator()(const ref<variable>& self, const type& rhs, UF& uf) const {
        // pp << "unifying: " << type(self) << " ~ " << rhs << std::endl;
        
        assert( uf.find(self) == self );
        assert( uf.find(rhs) == rhs );        
        
        if( type(self) != rhs && rhs.map<bool>(occurs_check<UF>(), self, uf)) {
          throw occurs_error{self, rhs.get< ref<application> >()};
        }

        if( self->kind != rhs.kind() ) {
          std::stringstream ss;

          pretty_printer(ss) << "when unifying: " << type(self) << " :: " << self->kind 
                             << " ~ " << rhs << " :: " << rhs.kind();
          
          throw kind_error(ss.str());
        }


        if( rhs.is<ref<variable> >() && self->depth < rhs.get< ref<variable> >()->depth) {
          // the topmost variable wins
          uf.link(rhs, self);
          return;
        }
        
        // debug( std::clog << "linking", type(self), rhs ) << std::endl;
        uf.link(self, rhs);
        
      }


      void operator()(const ref<constant>& lhs, const ref<constant>& rhs, UF& uf) const {
        // pp << "unifying: " << type(lhs) << " ~ " << type(rhs) << std::endl;

        if( lhs != rhs ) {
          throw unification_error(lhs, rhs);
        }
      }







      
      void operator()(const ref<application>& lhs, const ref<application>& rhs, UF& uf) const {
        // pp << "unifying: " << type(lhs) << " ~ " << type(rhs) << std::endl;        
        const auto indent = pp.indent();        

        uf.find(lhs->func).apply( unify_visitor(pp), uf.find(rhs->func), uf);
        uf.find(lhs->arg).apply( unify_visitor(pp), uf.find(rhs->arg), uf);
      }

      
    };


    void state::unify(const type& lhs, const type& rhs) {

      pretty_printer pp(std::clog);
      // pp << "unify: " << lhs << " ~ " << rhs << std::endl;
      // const auto indent = pp.indent();

      const type flhs = uf->find(lhs);
      const type frhs = uf->find(rhs);

      flhs.apply( unify_visitor<uf_type>(pp), frhs, *uf);
     
    }
    
    


    
    // type inference for expressions
    static inferred<type, ast::expr> infer(state& self, const ast::expr& node);

    static type infer(state& self, const ast::type& node);    

    struct type_visitor {

      using variables_type = std::map<symbol, ref<variable> >;
      
      type operator()(const ast::type_variable& self, state& tc) const {
        try {
          const scheme& p = tc.find(self.name);
          assert(p.body.is<ref<variable>>());
          return p.body;
        } catch( unbound_variable& ) {
          const ref<variable> a = tc.fresh();
          tc.def(self.name, tc.generalize(a));
          return a;
        }
      }

      type operator()(const ast::type_constructor& self, state& tc) const {
        auto it = tc.ctor->find(self.name);
        if(it == tc.ctor->end()) throw unbound_variable(self.name);
        return it->second;
      }

      
      type operator()(const ast::type_application& self, state& tc) const {
        return foldl(infer(tc, self.type), self.args, [&tc](const type& lhs, const ast::type& rhs) {
            return lhs(infer(tc, rhs));
          });
      }
      
      
    };

  
    
    static type infer(state& self, const ast::type& node) {
      try {
        return node.map<type>(type_visitor(), self);
      } catch( ... ) {
        std::cerr << "when inferring type for: " << repr(node) << std::endl;
        throw;
      }
    }
    

    // expression switch
    struct expr_visitor {


      // literals
      template<class T>
      inferred<type, ast::expr> operator()(const ast::literal<T>& self, state& tc) const {
        return {traits<T>::type(), self};
      }

      
      // variables
      inferred<type, ast::expr> operator()(const ast::variable& self, state& tc) const {
        const scheme& p = tc.find(self.name);
        const type res = tc.instantiate(p);
        return {res, self};
      }


      // lambdas
      inferred<type, ast::expr> operator()(const ref<ast::lambda>& self, state& tc) const {

        state sub = tc.scope();

        // create/define arg types
        const list< type > args = map(self->args, [&](const ast::lambda::arg& x) {

            // TODO typed variables
            const type a = tc.fresh();

            if(x.is<ast::lambda::typed>()) {
              const type b = infer(tc, x.get<ast::lambda::typed>().type);
              tc.unify(a, b);
            }

            
            // note: var stays monomorphic after generalization
            if(!(x.name() == kw::wildcard) ) {
              sub.def(x.name(), sub.generalize(a) );
            }
            
            return a;
          });


        // TODO FIXME this is to fix nullary applications until typed function
        // args are available
        if(size(self->args) == 1 && self->args->head.name() == kw::wildcard) {
          args->head = unit_type;
        }
        
        

        // infer body type in subcontext
        const inferred<type, ast::expr> body = infer(sub, self->body);

        // return complete application type
        const type res = foldr(body.type, args, [](const type& lhs, const type& rhs) {
            return lhs >>= rhs;
          });

        // and rewritten lambda body
        const ast::expr node = make_ref<ast::lambda>(self->args, body.node);
        
        return {res, node};
      }



      // applications
      inferred<type, ast::expr> operator()(const ref<ast::application>& self, state& tc) const {

        // TODO currying 
        
        const inferred<type, ast::expr> func = infer(tc, self->func);

        // infer arg types
        list< inferred<type, ast::expr> > args = map(self->args, [&](const ast::expr& e) {
            return infer(tc, e);
          });


        // construct function type
        const type result = tc.fresh();
        
        const type sig = foldr(result, args, [&](const inferred<type, ast::expr>& lhs, const type& rhs) {
            return lhs.type >>= rhs;
          });

        try{
          tc.unify(func.type, sig);
        } catch( occurs_error ) {
          throw type_error("occurs check");
        }

        // rewrite ast node
        const ast::expr node =
          make_ref<ast::application>(func.node, map(args, [](const inferred<type, ast::expr>& e) {
                return e.node;
              }));
        
        return {result, node};
      }


      // conditions
      inferred<type, ast::expr> operator()(const ast::condition& self, state& tc) const {
        const type result = tc.fresh();

        const ast::expr node = ast::condition{ map(self.branches, [&](const ast::branch& b) {
            const inferred<type, ast::expr> test = infer(tc, b.test);
            tc.unify(boolean_type, test.type);
            
            const inferred<type, ast::expr> value = infer(tc, b.value);
            tc.unify(value.type, result);

            return ast::branch(test.node, value.node);
            })} ;
        
        return {result, node};
      }


      // let-binding
      inferred<type, ast::expr> operator()(const ref<ast::definition>& self, state& tc) const {

        const type forward = tc.fresh();
        
        state sub = tc.scope();

        // note: value is bound in sub-context (monomorphic)
        sub.def(self->id, sub.generalize(forward));

        const inferred<type, ast::expr> value = infer(sub, self->value);       
        tc.unify(forward, value.type);

        tc.def(self->id, tc.generalize(value.type));

        const ast::expr node = make_ref<ast::definition>(self->id, value.node);

        return {io_ctor(unit_type), node};
      }


      // monadic binding
      inferred<type, ast::expr> operator()(const ref<ast::binding>& self, state& tc) const {

        const type forward = tc.fresh();
        
        // note: value is bound in sub-context (monomorphic)
        // note: monadic binding pushes a scope
        tc = tc.scope();
        
        tc.def(self->id, tc.generalize(forward));

        const inferred<type, ast::expr> value = infer(tc, self->value);
        
        tc.unify(io_ctor(forward), value.type);

        tc.find(self->id);

        // TODO should we rewrite as a def?
        const ast::expr node = make_ref<ast::binding>(self->id, value.node);
        return {io_ctor(unit_type), node};
        
      }
      

      // sequences
      inferred<type, ast::expr> operator()(const ast::sequence& self, state& tc) const {
        type res = io_ctor(unit_type);
        state sub = tc.scope();
        
        const ast::expr node = ast::sequence{ map(self.items, [&](const ast::expr& e) {
              res = io_ctor(tc.fresh()) ;
              
              const inferred<type, ast::expr> item = infer(sub, e);
              tc.unify(res, item.type);
              
              return item.node;
            })};
        
        return {res, node};
      }



      // fallback case
      inferred<type, ast::expr> operator()(const ast::expr& self, state& tc) const {
        std::stringstream ss;
        ss << "type inference unimplemented for " << self;
        throw error(ss.str());
      }
      
    };


    static inferred<type, ast::expr> infer(state& self, const ast::expr& node) {
      try{
        return node.map< inferred<type, ast::expr> >(expr_visitor(), self);
      } catch( unification_error& e )  {
        std::stringstream ss;

        pretty_printer pp(ss);
        pp << "when unifying \"" << e.lhs << "\" and \"" << e.rhs << "\"";
        
        throw type_error(ss.str());
      }
    }



    
    // finding nice representants (apply substitution)
    // TODO rename substitute
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




    
    const scheme& state::find(symbol id) const {

      if(scheme* p = env->find(id)) {
        return *p;
      }

      throw unbound_variable(id);
    }


    state& state::def(symbol id, const scheme& p) {
      auto res = env->locals.emplace( std::make_pair(id, p) );
      if(!res.second) throw error("redefinition of " + id.str());
      return *this;
    }
    

    ref<variable> state::fresh(kind k) const {
      return make_ref<variable>(k, env->depth);
    }


    state state::scope() const {

      // TODO should we use nested union-find too?
      ref<env_type> sub = make_ref<env_type>(env);

      return state( sub, uf, ctor);
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



  
    inferred<scheme, ast::toplevel> infer(state& self, const ast::toplevel& node) {

      static const int once = (self = self.scope(), 0);
      
      const inferred<type, ast::expr> res = infer(self, node.get<ast::expr>());

      const type mono = self.generalize(res.type).body;
      
      return {self.generalize(res.type), node};
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
