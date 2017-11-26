#include "types.hpp"

#include "sexpr.hpp"
#include "ast.hpp"

#include <algorithm>
#include <sstream>

namespace slip {

  namespace types {
    struct pretty_printer;
    
    static std::ostream& operator<<(std::ostream& out, const type& self);
    static type substitute( const union_find<type>& uf, const type& t);
    static void unify(pretty_printer& pp, union_find<type>& uf, type lhs, type rhs);
    
    ref<state::data_ctor_type> state::data_ctor = make_ref<data_ctor_type>();
    ref<state::ctor_type> state::ctor = make_ref<ctor_type>();    


    // variables
    struct vars_visitor {
      using result_type = std::set< variable >;
    
      void operator()(const constant&, result_type& ) const { }
      
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
          if(self.orig.kind == kinds::rows) return out << "...";
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
      
      void operator()(const constant& self, std::ostream& out, ostream_map&) const {
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







    // bool constructor::operator==(const constructor& other) const {
    //   return from == other.from && to == other.to;
    // }

    // bool constructor::operator<(const constructor& other) const {
    //   return from < other.from || (from == other.from && to < other.to);
    // }
    
    using kinds::terms;
    
    const type func_ctor = constant("->", terms >>= terms >>= terms );
    const type list_ctor = constant("list", terms >>= terms );
    const type io_ctor = constant("io", terms >>= terms );
    

    // row extension
    using kinds::rows;
    using kinds::kind;    
    
    static const kinds::kind row_extension_kind = terms >>= rows >>= rows;
    
    static type row_extension_ctor(symbol label) {
      return constant(label, row_extension_kind);
    }

    const type record_ctor = constant("record", rows >>= terms);
    const type empty_row_ctor = constant("{}", rows);
    

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
        return self.func.kind().get<kinds::constructor>().to;
      }  
    
    };
  

    kinds::kind type::kind() const {
      return apply(type_kind_visitor());
    }


    application::application(type func, type arg)
      : func(func),
        arg(arg) {
      using kinds::constructor;
      
      if(!func.kind().is<constructor>()) {
        throw kind_error("type constructor expected");
      }
      
      if(func.kind().get<constructor>().from != arg.kind() ) {
        std::stringstream ss;
        ss << "expected: " << func.kind().get<constructor>().from
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
      
      type operator()(const constant& self, const map_type& ) const {
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
    


    state::instantiate_type state::instantiate(const scheme& p) const {
      instantiate_visitor::map_type map;

      // associate each bound variable to a fresh one
      for(const variable& v : p.forall) {
        map.emplace(v, fresh(v.kind));
      }

      const type res = p.body.apply(instantiate_visitor(), map);
      scheme::constraints_type constraints;

      for(const type& c : p.constraints) {
        constraints.insert(c.apply(instantiate_visitor(), map));
      }
      
      return {res, constraints};
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



    struct occurs_check {
      using value_type = bool;
      using uf_type = union_find<type>;
      
      bool operator()(const constant&, const variable&, uf_type&) const {
        return false;
      }

      bool operator()(const variable& self, const variable& var, uf_type&) const {
        return self == var;
      }


      bool operator()(const application& self, const variable& var, uf_type& uf) const {
        // TODO union find in variables only + recursive call (like susbstitute?)
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
      void operator()(const constant&, UF&, pretty_printer&) const { }

      template<class UF>
      void operator()(const variable& self, UF& uf, pretty_printer& pp) const {
        (void) pp;
        if(self.depth > depth) {
          const type promoted = variable(self.kind, depth);
          // pp << "promoting: " << type(self) << " to: " << promoted << std::endl;
          uf.link(self, promoted);
        }
      }


      template<class UF>
      void operator()(const application& self, UF& uf, pretty_printer& pp) const {
        
        iter(self, [&](const type& x) {
            uf.find(x).apply(*this, uf, pp);
          });
        
      }
      
    };
    
    
    struct unify_visitor {

      using uf_type = union_find<type>;
      using pp_type = pretty_printer;
      
      const bool try_reverse;
      
      unify_visitor(bool try_reverse = true) 
        : try_reverse(try_reverse) { }
      
      // reverse dispatch
      template<class T>
      void operator()(const T& self, const type& rhs, uf_type& uf, pp_type& pp) const {
        // double dispatch
        if( try_reverse ) {
          return rhs.apply( unify_visitor(false), self, uf, pp);
        } else {
          throw unification_error(self, rhs);
        }
      }


      // variable / type
      void operator()(const variable& self, const type& rhs, uf_type& uf, pp_type& pp) const {
        const debug_unify debug(pp, self, rhs);
        
        assert( uf.find(self) == self );
        assert( uf.find(rhs) == rhs );        
        
        if( type(self) != rhs && rhs.apply(occurs_check(), self, uf)) {
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

        if(promote_rhs) {
          rhs.apply(promote_visitor{self.depth}, uf, pp);
        }
        
        uf.link(self, rhs);
      }


      void operator()(const constant& lhs, const constant& rhs, uf_type&, pp_type& pp) const {
        const debug_unify debug(pp, lhs, rhs);
        
        if(!(lhs == rhs )) {
          throw unification_error(lhs, rhs);
        }
      }



      // application / application
      void operator()(const application& lhs, const application& rhs, uf_type& uf, pp_type& pp) const {
        const debug_unify debug(pp, lhs, rhs);
        
        uf.find(lhs.func).apply( unify_visitor(), uf.find(rhs.func), uf, pp);

        // dispatch on kind
        if(lhs.arg.kind() == rows) {
          unify_rows(pp, uf, lhs.arg, rhs.arg);
        } else {
          // standard unification
          uf.find(lhs.arg).apply( unify_visitor(), uf.find(rhs.arg), uf, pp);
        }
        
      }

    };


    void unify(pretty_printer& pp, union_find<type>& uf, type lhs, type rhs) {
      const debug_unify debug(pp, lhs, rhs);
      
      lhs = uf.find(lhs);
      rhs = uf.find(rhs);
      
      lhs.apply( unify_visitor(), rhs, uf, pp);

      // pp << "lhs: " << substitute(*uf, lhs) << std::endl;
      // pp << "rhs: " << substitute(*uf, rhs) << std::endl;      
      
    }
    
    void state::unify(const type& lhs, const type& rhs) {
      pretty_printer pp(std::clog);
      types::unify(pp, *uf, lhs, rhs);
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
      void unify_diff(pretty_printer& pp, UF& uf, const row_helper& other) const {

        std::set<symbol> diff;
        std::set_difference(other.keys.begin(), other.keys.end(), keys.begin(), keys.end(), 
                            std::inserter(diff, diff.begin()));
        
        if(tail) {            
          type tmp = variable(tail->kind, tail->depth);
          for(symbol s : diff) {
            tmp = row_extension_ctor(s)(other.data.at(s))(tmp);
          }

          const debug_unify debug(pp, *tail, tmp);
          unify(pp, uf, *tail, tmp);
          
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
        // TODO unification debug
        std::set<symbol> both;
        
        // unify intersection
        std::set_intersection(lhs.keys.begin(), lhs.keys.end(), rhs.keys.begin(), rhs.keys.end(), 
                              std::inserter(both, both.begin()));
        
        for(symbol s : both) {
          lhs.data.at(s).apply(unify_visitor(), rhs.data.at(s), uf, pp);
        }

        lhs.unify_diff(pp, uf, rhs);
        rhs.unify_diff(pp, uf, lhs);
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
      // TODO debug here

      unify(pp, uf,
            row_helper(lhs),
            row_helper(rhs));
      
      // unify(pp, uf,
      //       row_helper(substitute(uf, lhs)),
      //       row_helper(substitute(uf, rhs)));
    };



    struct infer_expr_type {
      const struct type type;
      const scheme::constraints_type constraints;
      const ast::expr expr;
    };

    
    static infer_expr_type infer_expr(state& self, const ast::expr& node);
    static type infer_type(datatypes& self, const ast::type& node);    


    // expression switch
    struct expr_visitor {
      using value_type = infer_expr_type;

      // literals
      template<class T>
      infer_expr_type operator()(const ast::literal<T>& self, state&) const {
        return {traits<T>::type(), {}, self};
      }

      
      // variables
      infer_expr_type operator()(const ast::variable& self, state& tc) const {
        const scheme& p = tc.find(self.name);
        const state::instantiate_type res = tc.instantiate(p);
        
        return {res.type, res.constraints, self};
      }


      // lambdas
      infer_expr_type operator()(const ast::lambda& self, state& tc) const {

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
                const type unbox = sub.instantiate(dctor.unbox).type;
                
                sub.unify(unbox, outer >>= inner);
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
        const infer_expr_type body = infer_expr(sub, self.body);
        
        // return complete application type
        const type res = foldr(body.type, args, [&](const type& lhs, const type& rhs) {
            return lhs >>= rhs;
          });

        // and rewritten lambda body
        const ast::lambda expr = {self.args, body.expr};

        // return constraints yielded by body inference
        return {res, body.constraints, expr};
      }



      // applications
      infer_expr_type operator()(const ast::application& self, state& tc) const {

        // TODO currying 
        const infer_expr_type func = infer_expr(tc, self.func);

        // infer arg types
        list< infer_expr_type > args = map(self.args, [&](const ast::expr& e) {
            return infer_expr(tc, e);
          });


        // construct function type
        const type result = tc.fresh();
        
        const type sig = foldr(result, args, [&](const infer_expr_type& lhs, const type& rhs) {
            return lhs.type >>= rhs;
          });

        try{
          tc.unify(func.type, sig);
        } catch( occurs_error ) {
          throw type_error("occurs check");
        }

        // rewrite ast node
        const ast::application expr = {func.expr, map(args, [](const infer_expr_type& i) {
              return i.expr;
            })};


        // merge constraints TODO substitute constraints here?
        scheme::constraints_type constraints = func.constraints;
        for(const infer_expr_type& i : args) {
          for(const type& c : i.constraints) {
            constraints.insert(c);
          }
        }

        return {result, constraints, expr};
      }


      // conditions
      infer_expr_type operator()(const ast::condition& self, state& tc) const {
        const type result = tc.fresh();

        const ast::condition expr = { map(self.branches, [&](const ast::branch& b) {
              const infer_expr_type test = infer_expr(tc, b.test);
              tc.unify(boolean_type, test.type);
              
              const infer_expr_type value = infer_expr(tc, b.value);
              tc.unify(value.type, result);
              
              return ast::branch{test.expr, value.expr};
            })} ;
        
        return {result, {}, expr};
      }


      // let-binding
      infer_expr_type operator()(const ast::definition& self, state& tc) const {

        const type forward = tc.fresh();
        
        state sub = tc.scope();

        // note: value is bound in sub-context (monomorphic)
        // TODO constraints?
        sub.def(self.id, sub.generalize(forward));

        const infer_expr_type value = infer_expr(sub, self.value);       
        tc.unify(forward, value.type);

        // TODO constraints
        tc.def(self.id, tc.generalize(value.type));

        const ast::definition expr = {self.id, value.expr};

        return {io_ctor(unit_type), {}, expr};
      }


      // monadic binding
      infer_expr_type operator()(const ast::binding& self, state& tc) const {

        const type forward = tc.fresh();
        
        // note: value is bound in sub-context (monomorphic)
        // note: monadic binding pushes a scope
        tc = tc.scope();

        // TODO constraints
        tc.def(self.id, tc.generalize(forward));

        const infer_expr_type value = infer_expr(tc, self.value);
        
        tc.unify(io_ctor(forward), value.type);

        tc.find(self.id);

        // TODO should we rewrite as a def?
        const ast::binding expr = {self.id, value.expr};
        return {io_ctor(unit_type), {}, expr};
        
      }
      

      // sequences
      infer_expr_type operator()(const ast::sequence& self, state& tc) const {
        type res = io_ctor(unit_type);
        state sub = tc.scope();
        
        const ast::expr expr = ast::sequence{ map(self.items, [&](const ast::expr& e) {
              res = io_ctor(tc.fresh()) ;
              
              const infer_expr_type item = infer_expr(sub, e);
              tc.unify(res, item.type);
              
              return item.expr;
            })};
        
        return {res, {}, expr};
      }



      // record literals
      infer_expr_type operator()(const ast::record& self, state& tc) const {
        // TODO rewrite value terms
        const type row = foldr( empty_row_ctor, self.rows, [&tc](const ast::record::row& lhs, const type& rhs) {
            return row_extension_ctor(lhs.label)( infer_expr(tc, lhs.value).type )( rhs );
          });
        
        return {record_ctor(row), {}, self};
      }

      infer_expr_type operator()(const ast::selection& self, state& tc) const {
        const type alpha = tc.fresh();
        const type rho = tc.fresh( rows );
        const type row = row_extension_ctor(self.label)(alpha)(rho);
        
        return {record_ctor(row) >>= alpha, {}, self};
      }
      

      // module exports
      infer_expr_type operator()(const ast::export_& self, state& tc) const {
        const infer_expr_type value = infer_expr(tc, self.value);
        
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
          // TODO constraints
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

          return {target, {}, value.expr};
          
        } catch( std::out_of_range ) {
          throw type_error("unknown module: " + self.name.str());
        }
        
      }


      

      // fallback case
      infer_expr_type operator()(const ast::expr& self, state&) const {
        std::stringstream ss;
        ss << "type inference unimplemented for " << repr(self);
        throw error(ss.str());
      }
      
    };


    static infer_expr_type infer_expr(state& self, const ast::expr& node) {
      try{
        return node.apply(expr_visitor(), self);
      } catch( unification_error& e )  {
        std::stringstream ss;

        pretty_printer pp(ss);
        pp << "when unifying \"" << e.lhs << "\" and \"" << e.rhs << "\"";
        
        throw type_error(ss.str());
      }
    }



    
    // substitute all type variables
    struct substitute_visitor {
      using value_type = type;
      using uf_type = union_find<type>;
      
      type operator()(const constant& self, const uf_type&) const {
        return self;
      }

      type operator()(const variable& self, const uf_type& uf) const { 
        const type res = uf.find(self);
        if( res == self ) return res;
        return res.apply(substitute_visitor(), uf);
      }
      
      type operator()(const application& self, const uf_type& uf) const {
        return map(self, [&uf](const type& c) -> type {
            return c.apply(substitute_visitor(), uf);
          });
      }
      
    };


    static type substitute(const union_find<type>& uf, const type& t) {
      return uf.find(t).apply( substitute_visitor(), uf );
    }



    
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
    scheme state::generalize(const type& mono, const scheme::constraints_type& constraints) const {
      scheme res( substitute(*uf, mono) );

      const vars_visitor::result_type all = vars(res.body);
    
      // copy free variables (i.e. instantiated at a larger depth)
      auto out = std::inserter(res.forall, res.forall.begin());

      const std::size_t depth = this->env->depth;
      
      std::copy_if(all.begin(), all.end(), out, [depth](const variable& v) {
          return v.depth >= depth;
        });

      // insert (substituted) constraints
      for(const type& c : constraints) {
        res.constraints.insert( substitute(*uf, c) );
      }
      
      return res;
    }




    // type inference for type nodes   
    struct infer_type_visitor {
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
        return foldl(infer_type(ctors, self.ctor), self.args, [&ctors](const type& lhs, const ast::type& rhs) {
            return lhs(infer_type(ctors, rhs));
          });
      }
      
    };


    static type infer_type(datatypes& ctors, const ast::type& self) {
      try {
        return self.apply(infer_type_visitor(), ctors);
      } catch( error& e ) {
        std::cerr << "...when inferring type for: " << repr(self) << std::endl;
        ctors.debug( std::clog );
        throw;
      }
    }
    
    
    static std::ostream& operator<<(std::ostream& out, const type& self) {
      pretty_printer pp(out);
      pp << self;
      return out;
    }

    
    
    // fill kind environment from type environment
    struct fill_kind_visitor {

      using db_type = kinds::environment;
      
      void operator()(const ast::type_variable& self, db_type& db, const datatypes&) const {
        if( db.find(self.name) ) { }
        else db.locals.emplace(self.name, kinds::variable());
      }

      void operator()(const ast::type_constant& self, db_type& db, const datatypes& ctors) const {
        if( auto* t = ctors.find(self.name) ) { 
          db.locals.emplace(self.name, t->kind());
        } else {
          throw unbound_variable(self.name);
        }
      }

      void operator()(const ast::type_application& self, db_type& db, const datatypes& ctors) const {
        self.ctor.apply(fill_kind_visitor(), db, ctors);
        for(const ast::type& t : self.args) {
          t.apply(fill_kind_visitor(), db, ctors);
        }
      }
        
        
    };
    

    // fill kenv with kind variables and infer kinds
    static void fill_infer_kind(union_find<kind>& uf, kinds::environment& kenv, 
                                const ast::type& node, const datatypes& ctors) {
      node.apply(fill_kind_visitor(), kenv, ctors);
      infer_kind(uf, node, kenv);
    }


    static void fill_variables(datatypes& ctors, const kinds::environment& kenv, const union_find<kind>& uf,
                               std::size_t depth) {
      for(auto& it : kenv.locals) {
        if(it.second.is<kinds::variable>()) {
          const kind k = substitute(uf, it.second);
          ctors.locals.emplace(it.first, variable(k, depth)); // TODO hardcode
        }
      }

    }

    static void infer_module_arg_kinds(union_find<kind>& uf, kinds::environment& kenv,
                                       const datatypes& ctors, const ast::module& self) {
      // declare module args
      for(const ast::type_variable& v : self.args) { 
        fill_kind_visitor()(v, kenv, ctors);
      }
      
      // process all types in rows to gather information on module args
      for( const ast::module::row& r : self.rows ) {
        kinds::environment ksub(kenv);
        fill_infer_kind(uf, ksub, r.type, ctors);
      }
      
    }



    static infer_expr_type resolve_instance(const state& tc, const type& s) {
      

    }
    

    // context reduction
    static infer_expr_type reduce_context(state& tc, const infer_expr_type& src) {

      scheme::constraints_type reduced;

      try {
        
        for(const type& c : src.constraints) {
          // 1. substitute constraint type
          const type s = substitute(*tc.uf, c);

          // 2. obtain type class and instance type
          if(auto app = s.get_if<application>()) {

            // TODO expand multi parameters
            const constant tyclass = app->func.get<constant>();
            const type arg = app->arg;

            // 3. process constrained argument
            if(arg.is<variable>()) {
              // no further info on constrained type: just add it to the reduced
              // context
              reduced.insert(s);
            } else {
              // constant/application: we need to check an instance is available
              const infer_expr_type instance = resolve_instance(tc, s);
              
              // TODO use instance resolution to rewrite src.expr with
              // dictionary passing
              
              // TODO deal with constraint entailment if instance has constraints
              if( !instance.constraints.empty() ) {
                throw type_error("unimplemented: constraint entailment");
              }
              
            }          
          
          } else throw std::logic_error("constraint types must be applications");
        
        }
      
        return src;
      } catch( std::bad_cast ) {
        throw type_error("unimplemented: multi-parameter type classes");
      }
    };


    

    // toplevel visitor
    struct infer_toplevel_visitor {
      using value_type = inferred<scheme, ast::toplevel>;

      value_type operator()(const ast::expr& self, state& tc) {
        const infer_expr_type res = infer_expr(tc, self);
        const infer_expr_type reduced = reduce_context(tc, res);

        return {tc.generalize(reduced.type, reduced.constraints), reduced.expr};
      }


      // modules
      value_type operator()(const ast::module& self, state& tc) {

        // infer kinds for module arguments
        auto kenv = make_ref<kinds::environment>();
        union_find<kind> uf;

        infer_module_arg_kinds(uf, *kenv, *tc.ctor, self);
        
        // module type environment
        auto sub = make_ref<datatypes>(tc.ctor);        
        
        // declare type variables for module args and build constructor kind
        const kind k = foldr(terms, self.args, [&](const ast::type_variable& lhs, const kind& rhs) {
            const kind ki = substitute(uf, *kenv->find(lhs.name));
            
            if(ki.is<kinds::variable>()) {
              throw kind_error("could not infer kind for " + lhs.name.str());
            }

            // TODO FIXME current depth?
            const type ti = variable(ki, 0);
            sub->locals.emplace(lhs.name, ti);
            
            return ki >>= rhs;
          });
          
        // module type constructor
        const type ctor = constant(self.ctor.name, k);
        // TODO recursive definition?
 
        // typecheck rows and build unboxed record type
        const type rows = foldr(empty_row_ctor, self.rows, [&](const ast::module::row& lhs, const type& rhs) {
            // (re) infer kinds TODO don't do it twice
            kinds::environment ksub(kenv);
            fill_infer_kind(uf, ksub, lhs.type, *tc.ctor);
            
            // declare type variables in ssub based on inferred kinds
            datatypes ssub(sub);
            fill_variables(ssub, ksub, uf, 1); // TODO hardcoded depth
            return row_extension_ctor(lhs.name)( infer_type(ssub, lhs.type) ) (rhs);
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

        // generalize rank2 vars in unboxed type: we push a scope so that module
        // args remain monomorphic
        state ctx = tc.scope();
        const scheme source = ctx.generalize( unboxed );
        
        // std::clog << "unboxed: " << unboxed << std::endl;
        // std::clog << "source: " << source << std::endl;
        
        // data constructor
        data_constructor data = {source.forall, unbox};
        tc.data_ctor->emplace(self.ctor.name, data);

        // TODO this should be io unit
        return { tc.generalize( io_ctor(unit_type) ), ast::expr{ ast::sequence{} } };
        // return {tc.generalize(module), self};
      }
      
      
    };
    
  
    inferred<scheme, ast::toplevel> infer_toplevel(state& tc, const ast::toplevel& node) {
      return node.apply(infer_toplevel_visitor(), tc);
    }





    scheme::scheme(const type& body): body(body) {
      if(body.kind() != terms) {
        std::stringstream ss;
        ss << "expected: " << terms << ", got: " << body.kind();
        throw kind_error(ss.str());
      }
    }
    

    std::ostream& operator<<(std::ostream& out, const scheme& self) {
      pretty_printer pp(out);
      
      for(const variable& var : self.forall) {
        pp.osm.emplace( std::make_pair(var, pretty_printer::var{pp.osm.size(), true, var} ) );
      }

      if(!self.constraints.empty()) {
        for(const type& c : self.constraints) {
          pp << c << " ";
        }

        pp << "=> ";
      }
      
      pp << self.body;
      return out;
    }





  }
  
}
