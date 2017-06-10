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

#include "eval.hpp"

#include "tool.hpp"
#include "vm.hpp"
#include "syntax.hpp"

#include "jit.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;


template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    add_history(line);
    
	std::stringstream ss(line);
	f(ss);
  }
  
};




static const auto interpretr = [] {
  const ref<lisp::context> ctx = lisp::std_env();
  
  return [ctx](lisp::sexpr&& e) {
    const lisp::sexpr ex = expand_seq(ctx, e);
    const lisp::value val = eval(ctx, ex);
    
    if(val) {
      std::cout << val << std::endl;  
    }
    
    return parse::pure(e);
  };
};




template<class Action>
static int process(std::istream& in, Action action) {
  
  const auto parse = *( lisp::parser() >> action | parse::error<lisp::sexpr>() );
  
  try{
    parse(in);
    return 0;
  }
  catch(parse::error<lisp::sexpr>& e) {
    std::cerr << "parse error: " << e.what() << std::endl;
  }
  catch( lisp::syntax_error& e ) {
    std::cerr << "syntax error: " << e.what() << std::endl;
  }

  catch( lisp::type_error& e ) {
    std::cerr << "type error: " << e.what() << std::endl;
  }
  
  catch( lisp::error& e ) {
    std::cerr << "runtime error: " << e.what() << std::endl;
  }  

  return 1;
}



const auto jit_compiler = [](bool dump) {

  const ref<lisp::jit> jit = make_ref<lisp::jit>();
  const ref<lisp::context> env = lisp::std_env();
  
  jit->import( env );
  
  return [env, jit, dump](lisp::sexpr&& e) {

    const lisp::sexpr ex = expand_seq(env, e);    
    const lisp::vm::value val = jit->eval(ex, dump);

    if(val) {
      if(val.is<lisp::symbol>() || val.is<lisp::vm::value::list>()) {
        std::cout << "'";
      }
      std::cout << val << std::endl;  
    }
    return parse::pure(e);
  };
  
  

};

namespace po = boost::program_options;
static po::variables_map parse_options(int argc, char** argv) {
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("filename", po::value< std::string >(), "input file")

    ("i", "interpreter")
    ("b", "bytecode")
    ("dump", "dump bytecode")    
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

  using action_type = std::function< parse::any<lisp::sexpr> (lisp::sexpr&&) > ;
  
  action_type action = nullptr;

  
  if( vm.count("i") ) {
    action = interpretr();
  } else {
    const bool dump = vm.count("dump");    
    action = jit_compiler(dump);
  }
  

  
  if( vm.count("filename") ) {
    const std::string filename = vm["filename"].as< std::string >();
    std::ifstream file( filename );
    
    return process(file, action );
    
  } else {
  
    read_loop([&](std::istream& in) {
        process(in, action );
      });
    
  }
 
  return 0;
}
 
 
