// #define PARSE_ENABLE_DEBUG

#include <iostream>
#include "parse.hpp"

#include <sstream>
#include <string>

#include <set>
#include <memory>
#include <vector>
#include <deque>
#include <cassert>
#include <fstream>

#include <unordered_map>
#include <map>

#include <readline/readline.h>
#include <readline/history.h>

#include "tool.hpp"
#include "vm.hpp"

#include "jit.hpp"

#include "types.hpp"
#include "ast.hpp"

#include <numeric>


#include <boost/program_options.hpp>
namespace po = boost::program_options;


template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    if(!*line) continue;
    
    add_history(line);
	std::stringstream ss(line);
    
	f(ss);
  }
  
};



template<class Action>
static int process(std::istream& in, Action action) {
  in >> std::noskipws;
  
  try{
    const auto eof = parse::debug("eof") <<= parse::token(parse::eof(), slip::skipper());
    const auto error = parse::debug("error") <<= parse::error<parse::eof, slip::parse_error>();
    
    const auto parser = (*(slip::parser() >> action), eof | error);
    parser(in);
    return 0;
  }
  catch( slip::parse_error& e) {
    std::cerr << "parse error" << std::endl;
  }
  catch( slip::syntax_error& e ) {
    std::cerr << "syntax error: " << e.what() << std::endl;
  }

  catch( slip::type_error& e ) {
    std::cerr << "type error: " << e.what() << std::endl;
  }

  catch( slip::kind_error& e ) {
    std::cerr << "kind error: " << e.what() << std::endl;
  }

  
  catch( slip::error& e ) {
    std::cerr << "runtime error: " << e.what() << std::endl;
  }  

  return 1;
}



struct binder {
  ref<slip::jit>& jit;
  ref<slip::types::state>& tc;


  template<class Ret, class ... Args>
  static slip::types::type signature( Ret(*f)(Args...) ) {
    using namespace slip;
    using namespace types;    
    
	const type expand[] = { traits<Args>::type()... };
  
	const list<type> args = make_list<type>(expand, expand + sizeof...(Args));

	const type ret = traits<Ret>::type();
	return foldr( ret, args, [](const type& lhs, const type& rhs) {
		return lhs >>= rhs;
	  });
  }
  
  template<class F>
  binder& operator()(slip::symbol id, const F& f) {
    
	using namespace slip;
  
	jit->def(id, vm::wrap(f));
	tc->def(id, tc->generalize(signature(+f)));

	return *this;
  }
  
};



static const auto compiler = [](bool dump_bytecode) {
  using namespace slip;

  auto jit = make_ref<slip::jit>();
  auto tc = make_ref<types::state>();

  binder{jit, tc}
  ("+", [](integer x, integer y) -> integer { return x + y; })
  ("-", [](integer x, integer y) -> integer { return x - y; })
  ("*", [](integer x, integer y) -> integer { return x * y; })
  ("/", [](integer x, integer y) -> integer { return x / y; })
  ("%", [](integer x, integer y) -> integer { return x % y; })
  ("=", [](integer x, integer y) -> boolean { return x == y; })    
  ;

  
  using namespace types;
  {
    type a = tc->fresh();
    tc->def(kw::pure, tc->generalize( a >>= io_ctor(a) ));
    
    jit->def(kw::pure, +[](vm::stack* args) {
        return pop(args);
      });


    const type builtins[] = { traits< unit >::type(),
                              traits< boolean >::type(),
                              traits< integer >::type(),
                              traits< real >::type(),
                              traits< ref<string> >::type() };
                              
    for(const type& t : builtins) {
      tc->ctor->locals.emplace(t.get<constant>().name, t);
    }
    
    tc->ctor->locals.emplace(list_ctor.get<constant>().name, list_ctor);
    tc->ctor->locals.emplace(func_ctor.get<constant>().name, func_ctor);        
  }

  const type string_type = traits< ref<vm::string> >::type();
  const type unit_type = traits< vm::unit >::type();
  
  {
    // print
    tc->def("print", tc->generalize( string_type >>= io_ctor(unit_type)  ));
    jit->def("print", +[](vm::stack* args) -> vm::value {
        std::cout << *pop(args).get< ref<vm::string> >() << std::endl;
        return vm::unit();
      });
  }


  {
    // lists
    {
      type a = tc->fresh();
      tc->def("nil", tc->generalize( list_ctor(a)));
      jit->def("nil", vm::value::list());
    } 

    {
      type a = tc->fresh();
      tc->def("cons", tc->generalize(a >>= list_ctor(a) >>= list_ctor(a) ));
      jit->def("cons", +[](vm::stack* args) -> vm::value {
          const vm::value head = pop(args);
          const vm::value tail = pop(args);
          // TODO move
          return head >>= tail.get< vm::value::list >();
        });
    }
  }

  
  return [tc, jit, dump_bytecode](sexpr&& s) mutable {

    const ast::toplevel node = ast::check_toplevel(s);

    const types::inferred<types::scheme, ast::toplevel> p = infer(*tc, node);

    try {
      const vm::value v = jit->eval(p.node, dump_bytecode);

      std::cout << " : " << p.type
                << " = " << v << std::endl;
      
    } catch(...) {
      std::cout << " : " << p.type << std::endl;
      // throw;
    }

    return parse::pure(s);
  };
  
};


namespace po = boost::program_options;
static po::variables_map parse_options(int argc, char** argv) {
  po::options_description desc("allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("filename", po::value< std::string >(), "input file")
    ("dump", "dump bytecode")
    ("debug", "debug unification")
    ;

  po::positional_options_description p;
  p.add("filename", -1);
  
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  
  po::notify(vm);
  
  if (vm.count("help")) {
    std::cout << desc << "\n";
    std::exit(1);
  }
  
  return vm;
}




int main(int argc, char** argv) {

  const po::variables_map vm = parse_options(argc, argv);

  using action_type = std::function< parse::any<slip::sexpr> (slip::sexpr&&) > ;
  
  const bool dump = vm.count("dump");    
  action_type action = compiler(dump);

  if( vm.count("debug") ) {
    slip::types::debug_unification = true;
  }
  
  if( vm.count("filename") ) {
    const std::string filename = vm["filename"].as< std::string >();
    std::ifstream file( filename );

    process(file, action );
    
  } else {
  
    read_loop([&](std::istream& in) {
        process(in, action );
      });
    
  }
 
  return 0;
}
 
 
