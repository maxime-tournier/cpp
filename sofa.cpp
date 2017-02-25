#include "flow.hpp"
#include "variant.hpp"

#include <Eigen/Core>


#include <cstdlib>
#include <memory>
#include <cxxabi.h>

std::string demangle(const char* name) {

    int status = -4; // some arbitrary value to eliminate the compiler warning

    // enable c++11 by passing the flag -std=c++11 to g++
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };

    return (status==0) ? res.get() : name ;
}

template<int N = Eigen::Dynamic, class U = double>
using vector = Eigen::Matrix<U, N, Eigen::Dynamic>;

using vec1 = vector<1>;
using vec2 = vector<2>;
using vec3 = vector<3>;
using vec4 = vector<4>;
using vec6 = vector<6>;

template<class G> struct dofs;


struct dofs_base {
  virtual ~dofs_base() { }

  virtual void init() { }

  using cast_type = variant< dofs<double>,
							 dofs<vec1>,
							 dofs<vec2>,
							 dofs<vec3>,
							 dofs<vec4>,
							 dofs<vec6>>;
  
  virtual cast_type cast() = 0;
  
};

template<class G>
struct dofs : public dofs_base,
			  public std::enable_shared_from_this< dofs<G> > {
  G pos;

  dofs::cast_type cast() {
	return this->shared_from_this();
  }
  
};

template<class T>
struct mapping;

struct mapping_base {
  virtual ~mapping_base() { }

  virtual void init() { }

  using cast_type = variant< mapping<double (double) >,
							 mapping<vec1 (vec3)>,
							 mapping<vec1 (vec3, vec2)>
							 >;
  
  virtual cast_type cast() = 0;


  
};

template<class To, class ... From>
struct mapping< To (From...) > : public mapping_base,
								 public std::enable_shared_from_this< mapping< To (From...) > >{
  
  virtual void apply(To& to, const From&... from) const { };

  mapping::cast_type cast() {
	return this->shared_from_this();
  }

};


using vertex = variant<dofs_base, mapping_base>;
struct edge {};

struct graph : dependency_graph<vertex, edge> {

  template<class T, class ... Args>
  unsigned add_shared(Args&& ... args) {
	return add_vertex( std::make_shared<T>(std::forward<Args>(args)...), *this);
  }
  
};


struct typecheck {

  void operator()(dofs_base* self, unsigned v, const graph& g) const {
	
  }


  void operator()(mapping_base* self, unsigned v, const graph& g) const {
	self->cast().apply(*this, v, g);
  }



  struct type_error : std::runtime_error {
	using std::runtime_error::runtime_error;
  };

  struct edge_error : std::runtime_error {
	using std::runtime_error::runtime_error;
  };

  
  template<class Expected>
  struct expected_check {

	void operator()(dofs_base* self) const {
	  self->cast().apply(*this);
	}

	void operator()(mapping_base* self) const {
	  self->cast().apply(*this);
	}

	// ok cases
	void operator()(dofs<Expected>* ) const { };

	template<class ... From>
	void operator()(mapping<Expected (From...) >* ) const { };	


	template<class T>
	void operator()(dofs<T>* ) const {
	  throw std::runtime_error("type error: "+ demangle( typeid(Expected (T) ).name() ));
	}


	template<class T, class ... From>
	void operator()(mapping<T (From...) >* ) const {
	  throw std::runtime_error("type error: " + demangle( typeid(Expected (T) ).name() ));
	}
	
  };
  
  template<class Expected, class Iterator, class G>
  static void check_type(Iterator first, Iterator last, const G& g) {
	if(first == last) {
	  throw std::runtime_error("not enough out edges");
	}

	const unsigned v = *first;
	
	g[v].apply(expected_check<Expected>());
  }
  
  template<class To, class ... From>
  void operator()(mapping<To (From...)>* self, unsigned v, const graph& g) const {

	auto out = adjacent_vertices(v, g);
	try {
	  const int expand[] = { (check_type<From>(out.first++, out.second, g), 0)...};
	  
	  if(out.first != out.second) {
		throw std::runtime_error("too many out edges");
	  }
	} catch( std::runtime_error& e ){
	  throw std::runtime_error(e.what() + std::string(" when processing vertex: ") + std::to_string(v) );
	}
	
  }
  
};


int main(int, char**) {

  graph g;
  
  unsigned u = g.add_shared<dofs<vec3>>();
  unsigned v = g.add_shared<mapping<vec1 (vec3, vec2)>>();
  unsigned w = g.add_shared<dofs<vec2>>();
  
  add_edge(v, u, g);
  add_edge(v, w, g);
  
  std::vector<unsigned> order;
  g.sort(order);

  for(unsigned v : order) {
	g[v].apply( typecheck(), v, g );
  }
  
  g.update(order, [&](unsigned v) {
	  std::cout << "updated " << v << std::endl;
	  g[v].set<graph::clear>();
	});

  
}
 
