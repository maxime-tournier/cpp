#include "types.hpp"

#include "sexpr.hpp"
#include "ast.hpp"

#include <algorithm>
#include <sstream>

namespace slip {

  namespace types {
    static std::ostream& operator<<(std::ostream& out, const type& self);

    ref<state::data_ctor_type> state::data_ctor = make_ref<data_ctor_type>();
    ref<state::ctor_type> state::ctor = make_ref<ctor_type>();    


    // variables
    struct vars_visitor {
      using result_type = std::set< variable >;
    
      void operator()(const constant&, result_type& res) const { }
      
      void operator()(const variable& self, result_type& res) const {
        res.insert(self);
      }

      void operator()(const application& self, result_type& res) const {
        self.func.apply( vars_visitor(), res );
        self.arg.apply( vars_visitor(), res );      
      }
    
    };


    static vars_visitor::result_type vars(const type& self) {
      vars_visitor::result_type res;
      self.apply(vars_visitor(), res);
      return res;
    }
    
    
    
    // env lookup/def
    struct unbound_variable : type_error {
      unbound_variable(symbol id) 
        : type_error("unbound variable \"" + id.str() + "\"") { }
    };
    
    // pretty-printing types
    // TODO clean this shit
    struct pretty_printer {

      struct var {
        std::size_t index;
        bool quantified;

        variable orig;

        friend std::ostream& operator<<(std::ostream& out, const var& self) {
          if( self.quantified ) out << "'";
          else out << "!";
          return out << char('a' + self.index)
                     // << "[" << self.orig.depth << "]"
                     // << " :: " << self.orig.kind;
            ;
        }
        
      };

      using ostream_map = std::map< variable, var >;

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
        
        if(f == type(std::endl)) {
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
      
      void operator()(const variable& self, std::ostream& out, 
                      ostream_map& osm) const {
        auto err = osm.emplace(self, var{osm.size(), false, self});
        out << err.first->second;
      }
      
      void operator()(const application& self, std::ostream& out,
                      ostream_map& osm) const {

        // i have no idea what i'm doing lol

        if(self.func == func_ctor) {
          self.arg.apply(ostream_visitor(true), out, osm);
          out << ' ';
          self.func.apply(ostream_visitor(true), out, osm);
        } else {
          if(parens) out << '(';
          // const bool sub = self.func.is< ref<application> >();
          
          self.func.apply(ostream_visitor(true), out, osm);
          out << ' ';
          self.arg.apply(ostream_visitor(false), out, osm);
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
      return {lhs, rhs};
    }


    bool constructor::operator==(const constructor& other) const {
      return from == other.from && to == other.to;
    }

    bool constructor::operator<(const constructor& other) const {
      return from < other.from || (from == other.from && to < other.to);
    }
    
    
    const type func_ctor = constant("->", terms() >>= terms() >>= terms() );
    const type list_ctor = constant("list", terms() >>= terms() );
    const type io_ctor = constant("io", terms() >>= terms() );
    

    // row extension
    const kind row_extension_kind = terms() >>= rows() >>= rows();
    static type row_extension_ctor(symbol label) {
      return constant(label, row_extension_kind);
    }

    const type record_ctor = constant("record", rows() >>= terms());
    const type empty_row_ctor = constant("{}", rows());
    

    type type::operator()(const type& arg) const {
      return application(*this, arg);
    }
    
    type operator>>=(const type& lhs, const type& rhs) {
      return (func_ctor(lhs))(rhs);

    }
    

    struct type_kind_visitor {
      using value_type = kind;
      
      kind operator()(const constant& self) const { return self.kind; }
      kind operator()(const variable& self) const { return self.kind; }    
      kind operator()(const application& self) const  {
        return self.func.kind().get<constructor>().to;
      }  
    
    };
  

    struct kind type::kind() const {
      return apply(type_kind_visitor());
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
        out << self.from << " -> " << self.to;
      }


      void operator()(const kind_variable&, std::ostream& out) const {
        out << '?';
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
      
      if(func.kind().get<constructor>().from != arg.kind() ) {
        std::stringstream ss;
        ss << "expected: " << func.kind().get< constructor >().from
           << ", got: " << arg.kind();
           
        throw kind_error(ss.str());
      }
    }



    bool application::operator==(const application& other) const {
      return func == other.func && arg == other.arg;
    }
    
    bool application::operator<(const application& other) const {
      return func < other.func || (func == other.func && arg < other.arg);
    }
    


    
    // term types
    const type unit_type = constant("unit"),
              boolean_type = constant("boolean"),
              integer_type = constant("integer"),
              real_type = constant("real"),
              string_type = constant("string"),
              symbol_type = constant("symbol");
    

  
    template<> type traits< slip::unit >::type() { return unit_type; }
    template<> type traits< slip::boolean >::type() { return boolean_type; }      
    template<> type traits< slip::integer >::type() { return integer_type; }
    template<> type traits< slip::real >::type() { return real_type; }
    template<> type traits< slip::symbol >::type() { return symbol_type; }

    // mew
    template<> type traits< ref<slip::string> >::type() { return string_type; }
    

    state::state(ref<env_type> env, ref<uf_type> uf) 
      : env( env ),
        uf( uf ) {
      
    }



    // instantiation
    struct instantiate_visitor {
      using value_type = type;
      
      using map_type = std::map< variable, variable >;
      
      type operator()(const constant& self, const map_type& m) const {
        return self;
      }

      type operator()(const variable& self, const map_type& m) const {
        auto it = m.find(self);
        if(it == m.end()) return self;
        return it->second;
      }

      type operator()(const application& self, const map_type& m) const {
        return map(self, [&](const type& c) {
            return c.apply(instantiate_visitor(), m);
          });
      }
      
    };
    


    type state::instantiate(const scheme& p) const {
      instantiate_visitor::map_type map;

      // associate each bound variable to a fresh one
      for(const variable& v : p.forall) {
        map.emplace(v, fresh(v.kind));
      }
      
      return p.body.apply(instantiate_visitor(), map);
    }


    

    
    // unification
    struct unification_error {
      unification_error(type lhs, type rhs) : lhs(lhs), rhs(rhs) { }
      const type lhs, rhs;
    };

    
    struct occurs_error { // : unification_error
      occurs_error(const variable& var, const struct type& type)
        : var(var), type(type) {

      }
      
      variable var;
      struct type type;
    };



    template<class UF>
    struct occurs_check {
      using value_type = bool;
      
      bool operator()(const constant& self, const variable& var, UF& uf) const {
        return false;
      }

      bool operator()(const variable& self, const variable& var, UF& uf) const {
        return self == var;
      }


      bool operator()(const application& self, const variable& var, UF& uf) const {

        return
          uf.find(self.arg).apply(occurs_check(), var, uf) ||
          uf.find(self.func).apply(occurs_check(), var, uf);
      }
      
    };


    
    

    template<class UF>
    static void unify_rows(pretty_printer& pp, UF& uf, const type& lhs, const type& rhs);
    
    bool debug_unification = false;
    
    struct debug_unify {
      pretty_printer& pp;
      pretty_printer::indent_type indent;

      debug_unify(pretty_printer& pp, const type& lhs, const type& rhs)
        : pp( debug_unification ? (pp << "unify: " << type(lhs) << "  ~  " << type(rhs) << std::endl) : pp),
          indent(pp.indent()) {
        
      }
      
    };


    struct promote_visitor {
      // make sure all type variables in a type unify with a variable with given
      // depth
      std::size_t depth;

      template<class UF>
      void operator()(const constant& self, UF& uf) const { }

      template<class UF>
      void operator()(const variable& self, UF& uf) const {
        if(self.depth > depth) {
          uf.link(self, variable(self.kind, depth));
        }
      }


      template<class UF>
      void operator()(const application& self, UF& uf) const {
        
        iter(self, [&](const type& x) {
            uf.find(x).apply(*this, uf);
          });
        
      }
      
    };
    
    
    template<class UF>
    struct unify_visitor {

      pretty_printer& pp;
      const bool try_reverse;
      
      unify_visitor(pretty_printer& pp, bool try_reverse = true) 
        : pp(pp),
          try_reverse(try_reverse) { }

      // reverse dispatch
      template<class T>
      void operator()(const T& self, const type& rhs, UF& uf) const {
        // double dispatch
        if( try_reverse ) {
          return rhs.apply( unify_visitor(pp, false), self, uf);
        } else {
          throw unification_error(self, rhs);
        }
      }


      // variable / type
      void operator()(const variable& self, const type& rhs, UF& uf) const {
        const debug_unify debug(pp, self, rhs);
        
        assert( uf.find(self) == self );
        assert( uf.find(rhs) == rhs );        
        
        if( type(self) != rhs && rhs.apply(occurs_check<UF>(), self, uf)) {
          throw occurs_error{self, rhs.get<application>()};
        }

        // kind preserving unification
        if( self.kind != rhs.kind() ) {
          std::stringstream ss;
          
          pretty_printer(ss) << "when unifying: " << type(self) << " :: " << self.kind 
                             << "  ~  " << rhs << " :: " << rhs.kind();
          
          throw kind_error(ss.str());
        }

        // we will link self to rhs, but we might need to promote rhs to
        // self.depth (if it's deeper or a type application)
        const bool promote_rhs = rhs.is<application>() ||
          (rhs.is<variable>() && rhs.get<variable>().depth > self.depth);

        if(promote_rhs) rhs.apply(promote_visitor{self.depth}, uf);
        
        uf.link(self, rhs);
        
      }


      void operator()(const constant& lhs, const constant& rhs, UF& uf) const {
        const debug_unify debug(pp, lhs, rhs);
        
        if(!(lhs == rhs )) {
          throw unification_error(lhs, rhs);
        }
      }



      // application / application
      void operator()(const application& lhs, const application& rhs, UF& uf) const {
        const debug_unify debug(pp, lhs, rhs);
        
        uf.find(lhs.func).apply( unify_visitor(pp), uf.find(rhs.func), uf);

        // dispatch on kind
        if(lhs.arg.kind() == rows()) {
          unify_rows(pp, uf, lhs.arg, rhs.arg);
        } else {
          // standard unification
          uf.find(lhs.arg).apply( unify_visitor(pp), uf.find(rhs.arg), uf);
        }
        
      }

    };


    void state::unify(const type& lhs, const type& rhs) {
      pretty_printer pp(std::clog);
      const debug_unify debug(pp, lhs, rhs);
      
      const type flhs = uf->find(lhs);
      const type frhs = uf->find(rhs);

      flhs.apply( unify_visitor<uf_type>(pp), frhs, *uf);
     
    }
    





    class row_helper {
      std::map<symbol, type> data;
      std::set<symbol> keys;
      std::unique_ptr<variable> tail;
    public:
      
      row_helper(type row) {
        while(row.is< application >() ) {
          application app = row.get< application >();
          
          symbol label = app.func.get<application>().func.get<constant>().name;
          type tau = app.func.get<application>().arg;
          
          data.emplace(label, tau);
            
          row = app.arg;
        }

        if(row.is< variable >() ) {
          tail.reset( new variable(row.get<variable>()) );
        }

        for(const auto& it : data) keys.insert(it.first);
      }

      template<class UF>
      void unify_diff(UF& uf, const row_helper& other) const {

        std::set<symbol> diff;
        std::set_difference(other.keys.begin(), other.keys.end(), keys.begin(), keys.end(), 
                            std::inserter(diff, diff.begin()));
        
        if(tail) {            
          type tmp = variable(tail->kind, tail->depth);
          for(symbol s : diff) {
            tmp = row_extension_ctor(s)(other.data.at(s))(tmp);
          }
          
          uf.link(*tail, tmp);
        } else if(!diff.empty()) {
          std::stringstream ss;
          ss << "the following attributes cannot be matched:";
          for(const symbol& s : diff) {
            ss << " " << s;
          }
          throw type_error(ss.str());
        }
        
      }
      
      
      template<class UF>
      friend void unify(pretty_printer& pp, UF& uf, const row_helper& lhs, const row_helper& rhs)  {
        std::set<symbol> both;
        
        // unify intersection
        std::set_intersection(lhs.keys.begin(), lhs.keys.end(), rhs.keys.begin(), rhs.keys.end(), 
                              std::inserter(both, both.begin()));
        
        for(symbol s : both) {
          lhs.data.at(s).apply(unify_visitor<UF>(pp), rhs.data.at(s), uf);
        }

        lhs.unify_diff(uf, rhs);
        rhs.unify_diff(uf, lhs);
      }
      
      
      friend std::ostream& operator<<(std::ostream& out, const row_helper& self) {
        for(const auto& it : self.data) {
          out << "row: " << it.first << std::endl;
        }
        return out;
      }
    };


    
    template<class UF>
    static void unify_rows(pretty_printer& pp, UF& uf, const type& lhs, const type& rhs) {
      unify(pp, uf, row_helper(lhs), row_helper(rhs));
    };



    

    

    static inferred<type, ast::expr> infer(state& self, const ast::expr& node);
    static type infer(datatypes& self, const ast::type& node);    
    


    // expression switch
    struct expr_visitor {
      using value_type = inferred<type, ast::expr>;

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
      inferred<type, ast::expr> operator()(const ast::lambda& self, state& tc) const {

        state sub = tc.scope();

        // create/define (outer) arg types
        const list< type > args = map(self.args, [&](const ast::lambda::arg& x) {

            // typed lambda variables: use data constructor unbox function to
            // deduce inner/outer types
            if(x.is<ast::lambda::typed>()) {
              
              const data_constructor& dctor = tc.data_ctor->at(x.get<ast::lambda::typed>().ctor);

              // obtain outer as target type, instantiated at the calling level
              const type outer = tc.fresh();
              
              if(!(x.name() == kw::wildcard) ) {
                // inner type is instantiated at called level, unified with
                // unbox application with outer type, and generalized
                const type inner = sub.fresh();
                tc.unify(sub.instantiate(dctor.unbox), outer >>= inner);
                sub.def(x.name(), sub.generalize(inner) );
              }
              
              return outer;
            } else {
              const type a = tc.fresh();
              
              // note: var stays monomorphic after generalization              
              if(!(x.name() == kw::wildcard) ) {
                sub.def(x.name(), sub.generalize(a) );
              }

              return a;
            }

          });


        // TODO FIXME this is to fix nullary applications until typed function
        // args are available
        if(size(self.args) == 1 && self.args->head.name() == kw::wildcard) {
          args->head = unit_type;
        }
        
        

        // infer body type in subcontext
        const inferred<type, ast::expr> body = infer(sub, self.body);

        // return complete application type
        const type res = foldr(body.type, args, [](const type& lhs, const type& rhs) {
            return lhs >>= rhs;
          });

        // and rewritten lambda body
        const ast::expr node = ast::lambda(self.args, body.node);
        
        return {res, node};
      }



      // applications
      inferred<type, ast::expr> operator()(const ast::application& self, state& tc) const {

        // TODO currying 
        const inferred<type, ast::expr> func = infer(tc, self.func);

        // infer arg types
        list< inferred<type, ast::expr> > args = map(self.args, [&](const ast::expr& e) {
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
        const ast::application node = {func.node, map(args, [](const inferred<type, ast::expr>& e) {
              return e.node;
            })};
        
        return {result, node};
      }


      // conditions
      inferred<type, ast::expr> operator()(const ast::condition& self, state& tc) const {
        const type result = tc.fresh();

        const ast::condition node = { map(self.branches, [&](const ast::branch& b) {
              const inferred<type, ast::expr> test = infer(tc, b.test);
              tc.unify(boolean_type, test.type);
              
              const inferred<type, ast::expr> value = infer(tc, b.value);
              tc.unify(value.type, result);
              
              return ast::branch{test.node, value.node};
            })} ;
        
        return {result, node};
      }


      // let-binding
      inferred<type, ast::expr> operator()(const ast::definition& self, state& tc) const {

        const type forward = tc.fresh();
        
        state sub = tc.scope();

        // note: value is bound in sub-context (monomorphic)
        sub.def(self.id, sub.generalize(forward));

        const inferred<type, ast::expr> value = infer(sub, self.value);       
        tc.unify(forward, value.type);

        tc.def(self.id, tc.generalize(value.type));

        const ast::definition node = {self.id, value.node};

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
        const ast::binding node = {self->id, value.node};
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



      // record literals
      inferred<type, ast::expr> operator()(const ast::record& self, state& tc) const {
        // TODO rewrite value terms
        const type row = foldr( empty_row_ctor, self.rows, [&tc](const ast::record::row& lhs, const type& rhs) {
            return row_extension_ctor(lhs.label)( infer(tc, lhs.value).type )( rhs );
          });
        
        return {record_ctor(row), self};
      }

      inferred<type, ast::expr> operator()(const ast::selection& self, state& tc) const {
        const type alpha = tc.fresh();
        const type rho = tc.fresh( rows() );
        const type row = row_extension_ctor(self.label)(alpha)(rho);
        
        return {record_ctor(row) >>= alpha, self};
      }
      

      // module exports
      inferred<type, ast::expr> operator()(const ast::export_& self, state& tc) const {
        const inferred<type, ast::expr> value = infer(tc, self.value);
        
        try {
          // fetch associated data constructor
          const data_constructor& dctor = tc.data_ctor->at(self.name);

          // manually instantiate unbox type to keep track of variables
          instantiate_visitor::map_type map;
          for(const variable& v : dctor.unbox.forall) {
            map.emplace(v, tc.fresh(v.kind));
          }
          
          const type unbox = dctor.unbox.body.apply(instantiate_visitor(), map);

          // obtain source/target types
          const type source = tc.fresh();
          const type target = tc.fresh();

          // std::clog << "unbox: " << unbox << std::endl;          
          // std::clog << "value: " << value.type << std::endl;
          // std::clog << "depth: " << tc.env->depth << std::endl;
          
          tc.unify(unbox, target >>= source);
          tc.unify(value.type, source);

          // generalize source
          const scheme p = tc.generalize(source);
          // std::clog << "source: " << p << std::endl;
          
          // all the variables in dctor.forall must remain quantified in p
          for(const variable& v : dctor.forall) {
            assert(map.find(v) != map.end());

            const type sv = tc.uf->find( map.at(v) );
            
            if( !sv.is<variable>() || p.forall.find( sv.get<variable>() ) == p.forall.end() ) {
              // TODO more details
              throw type_error("generalization error");
            }
          }

          return {target, value.node};
          
        } catch( std::out_of_range ) {
          throw type_error("unknown module: " + self.name.str());
        }
        
      }


      

      // fallback case
      inferred<type, ast::expr> operator()(const ast::expr& self, state& tc) const {
        std::stringstream ss;
        ss << "type inference unimplemented for " << repr(self);
        throw error(ss.str());
      }
      
    };


    static inferred<type, ast::expr> infer(state& self, const ast::expr& node) {
      try{
        return node.apply(expr_visitor(), self);
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
      using value_type = type;
      
      template<class UF>
      type operator()(const constant& self, UF& uf) const {
        return self;
      }


      template<class UF>
      type operator()(const variable& self, UF& uf) const {

        const type res = uf->find(self);
        
        if(res == type(self)) {
          // debug(std::clog << "nice: ", type(self), res) << std::endl;          
          return res;
        }
        
        return res.apply(nice(), uf);
      }


      template<class UF>
      type operator()(const application& self, UF& uf) const {
        return map(self, [&](const type& c) {
            return c.apply(nice(), uf);
          });
      }
      
    };






    
    const scheme& state::find(symbol id) const {
      if(auto p = env->find(id)) return *p;
      throw unbound_variable(id);
    }


    state& state::def(symbol id, const scheme& p) {
      auto res = env->locals.emplace( std::make_pair(id, p) );
      if(!res.second) throw error("redefinition of " + id.str());
      return *this;
    }
    

    variable state::fresh(kind k) const {
      return {k, env->depth};
    }


    state state::scope() const {

      // TODO should we use nested union-find too?
      ref<env_type> sub = make_ref<env_type>(env);

      return state( sub, uf );
    }
    




    // generalization
    scheme state::generalize(const type& mono) const {
      scheme res(mono.apply(nice(), uf));

      const vars_visitor::result_type all = vars(res.body);
    
      // copy free variables (i.e. instantiated at a larger depth)
      auto out = std::inserter(res.forall, res.forall.begin());

      const std::size_t depth = this->env->depth;
      
      std::copy_if(all.begin(), all.end(), out, [depth](const variable& v) {
          return v.depth >= depth;
        });

      return res;
    }




    // type inference for type nodes   
    struct type_visitor {
      using value_type = type;

      // variables/constants: lookup
      template<class T>
      type operator()(const T& self, datatypes& ctors) const {
        if(auto t = ctors.find(self.name)) {
          return *t;
        }
        
        throw unbound_variable(self.name);
      }

      // applications
      type operator()(const ast::type_application& self, datatypes& ctors) const {
        return foldl(infer(ctors, self.ctor), self.args, [&ctors](const type& lhs, const ast::type& rhs) {
            return lhs(infer(ctors, rhs));
          });
      }
      
    };


    static type infer(datatypes& ctors, const ast::type& self) {
      try {
        return self.apply(type_visitor(), ctors);
      } catch( error& e ) {
        std::cerr << "...when inferring type for: " << repr(self) << std::endl;
        ctors.debug( std::clog );
        throw;
      }
    }
    
    
    struct infer_kind_visitor {
      using value_type = kind;
      
      template<class T, class DB, class UF>
      kind operator()(const T& self, const DB& db, UF& uf) const {
        if(auto* k = db.find(self.name)) {
          return *k;
        }

        throw unbound_variable(self.name);
      }

      template<class DB, class UF>
      kind operator()(const ast::type_application& self, const DB& db, UF& uf) const {

        const kind func = self.ctor.apply( infer_kind_visitor(), db, uf);
        const kind result = kind_variable();

        const kind call =
          foldr(result, self.args, [&](const ast::type& lhs, const kind& rhs) {
              const kind result = lhs.apply(infer_kind_visitor(), db, uf) >>= rhs;
              return result;
            });
        
        unify_kinds(uf, func, call);
        
        return result;
      }

      template<class UF>
      static void unify_kinds(UF& uf, kind lhs, kind rhs) {
        // std::clog << "unifying kinds: " << lhs << " and: " << rhs << std::endl;
        
        lhs = uf.find(lhs);
        rhs = uf.find(rhs);
        
        if(lhs.is<constructor>() && rhs.is<constructor>()) {
          unify_kinds(uf, lhs.get<constructor>().from, rhs.get<constructor>().from);
          unify_kinds(uf, lhs.get<constructor>().to, rhs.get<constructor>().to);
          return;
        }

        if(lhs.is<kind_variable>() || rhs.is<kind_variable>()) {
          const kind_variable& var =  lhs.is<kind_variable>() ?
            lhs.get<kind_variable>() : rhs.get<kind_variable>();
          const kind& other =  lhs.is<kind_variable>() ? rhs : lhs;

          // note: other becomes representant
          uf.link(var, other);

          return;
        }

        if(lhs != rhs) {
          std::stringstream ss;
          ss << lhs << " vs. " << rhs;
          throw kind_error(ss.str());
        }
        
      }
      
    };


    static std::ostream& operator<<(std::ostream& out, const type& self) {
      pretty_printer pp(out);
      pp << self;
      return out;
    }

    
    

    // environment for kinding
    struct kind_db : context<kind_db, kind>{
      using kind_db::context::context;

      struct fill_visitor {

        void operator()(const ast::type_variable& self, kind_db& db, const datatypes& ctors) const {
          if( db.find(self.name) ) { }
          else db.locals.emplace(self.name, kind_variable());
        }

        void operator()(const ast::type_constant& self, kind_db& db, const datatypes& ctors) const {
          if( auto* t = ctors.find(self.name) ) { 
            db.locals.emplace(self.name, t->kind());
          } else {
            throw unbound_variable(self.name);
          }
        }

        void operator()(const ast::type_application& self, kind_db& db, const datatypes& ctors) const {
          self.ctor.apply(fill_visitor(), db, ctors);
          for(const ast::type& t : self.args) {
            t.apply(fill_visitor(), db, ctors);
          }
        }
        
        
      };
      
      void fill(const datatypes& ctors, const ast::type& t) {
        t.apply( fill_visitor(), *this, ctors);
      }

    };

    // substitute all kind variables in a given kind
    template<class UF>
    struct substitute_kind_visitor {
      using value_type = kind;

      template<class T>
      kind operator()(const T& self, const UF& uf) const { return self; }

      kind operator()(const kind_variable& self, const UF& uf) const {
        return uf.find(self);
      }

      kind operator()(const constructor& self, const UF& uf) const {
        return constructor{ self.from.apply( substitute_kind_visitor(), uf),
            self.to.apply( substitute_kind_visitor(), uf) };
      }
      
    };

    template<class UF>
    static kind substitute(const kind& k, const UF& uf) {
      return uf.find(k).apply(substitute_kind_visitor<UF>(), uf);
    }
    
    
    // toplevel visitor
    struct toplevel_visitor {
      using value_type = inferred<scheme, ast::toplevel>;

      value_type operator()(const ast::expr& self, state& tc) {
        const inferred<type, ast::expr> res = infer(tc, self);        
        return {tc.generalize(res.type), self};
      }


      // modules
      value_type operator()(const ast::module& self, state& tc) {

        // infer kind for module arguments
        ref<kind_db> db = make_ref<kind_db>();
        union_find<kind> uf;
        
        // declare stuff
        for(const ast::type_variable& v : self.args) {
          db->fill(*tc.ctor, v);
        }

        {
          // process all types to gather information
          auto ksub = make_ref<kind_db>(db);
          for( const ast::module::row& r : self.rows ) {
            ksub->fill(*tc.ctor, r.type);
            r.type.apply(infer_kind_visitor(), *ksub, uf);
          }
        }

        // at this point we should have sufficent information for kinding
        // parameters
        
        // extract type variables and build kind
        auto sub = make_ref<datatypes>(tc.ctor);        
        
        // modules are values
        const kind init = terms();

        // 
        const kind k = foldr(init, self.args, [&](const ast::type_variable& lhs, const kind& rhs) {
            const kind ki = substitute(*db->find(lhs.name), uf);
            
            if(ki.is<kind_variable>()) {
              throw kind_error("could not infer kind for " + lhs.name.str());
            }

            // TODO current depth?
            const type ai = variable(ki, 0);
            
            if( !sub->locals.emplace(lhs.name, ai).second ) {
              throw type_error("duplicate type variable");
            }
            
            return ki >>= rhs;
          });
          
        // type constructor
        const type ctor = constant(self.ctor.name, k);
        // TODO recursive definition?
 
        // typecheck rows and build unboxed type
        state ctx = tc.scope();

        const type rows = foldr(empty_row_ctor, self.rows, [&](const ast::module::row& lhs, const type& rhs) {
            auto ssub = make_ref<datatypes>(sub);

            // (re) infer kinds TODO don't do it twice
            auto ksub = make_ref<kind_db>(db);
            ksub->fill(*tc.ctor, lhs.type);
            lhs.type.apply(infer_kind_visitor(), *ksub, uf);

            // define type variables in ssub based on inferred kinds
            for(auto& it : ksub->locals) {
              if(it.second.is<kind_variable>()) {
                const kind k = substitute(it.second, uf);
                std::clog << it.first  << ": " << k << std::endl;
                ssub->locals.emplace(it.first, variable(k, 1));
              }
            }
            
            return row_extension_ctor(lhs.name)( infer(*ssub, lhs.type) ) (rhs);
          });
        
        const type unboxed = record_ctor(rows);

        // register type constructor
        if(!tc.ctor->locals.emplace(self.ctor.name, ctor).second) {
          throw type_error("module redefinition");
        }

        // module type (ctor applied to its variables)
        const type module = foldl(ctor, self.args, [&](const type& lhs, const ast::type& rhs) {
            return lhs(sub->locals.at(rhs.get<ast::type_variable>().name));
          });

        const scheme unbox = tc.generalize( module >>= unboxed );

        // TODO 
        // std::clog << "unbox: " << unbox << std::endl;

        // generalize rank2 vars in unboxed type
        const scheme source = ctx.generalize( unboxed );
        std::clog << "unboxed: " << unboxed << std::endl;
        std::clog << "source: " << source << std::endl;
        
        // data constructor
        data_constructor data = {source.forall, unbox};
        
        tc.data_ctor->emplace(self.ctor.name, data);

        // TODO this should be io unit
        return {tc.generalize(module), self};
      }
      
      
    };
    
  
    inferred<scheme, ast::toplevel> infer(state& tc, const ast::toplevel& node) {
      return node.apply(toplevel_visitor(), tc);
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
      
      for(const variable& var : self.forall) {
        pp.osm.emplace( std::make_pair(var, pretty_printer::var{pp.osm.size(), true, var} ) );
      }
      
      pp << self.body;
      return out;
    }





  }
  
}
