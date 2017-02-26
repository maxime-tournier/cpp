#include "flow.hpp"
#include "variant.hpp"
#include "indices.hpp"

#include <Eigen/Core>
#include <cassert>

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


using real = double;

template<int N = Eigen::Dynamic, class U = real>
using vector = Eigen::Matrix<U, N, 1>;

using vec1 = vector<1>;
using vec2 = vector<2>;
using vec3 = vector<3>;
using vec4 = vector<4>;
using vec6 = vector<6>;

template<class G> struct dofs;

template<class G> struct traits;

template<int N, class U>
struct traits< vector<N, U> > {
  static std::size_t size(const vector<N, U>& ) {
	return N;
  }
};


template<>
struct traits<real> {
  static std::size_t size(const real& ) {
	return 1;
  }
};


struct dofs_base {
  virtual ~dofs_base() { }

  virtual void init() { }

  using cast_type = variant< 
	dofs<real>,
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

  dofs_base::cast_type cast() {
	return this->shared_from_this();
  }
  
};

template<class T>
struct func;

struct func_base {
  virtual ~func_base() { }

  virtual void init() { }

  using cast_type = variant< 
	func<real (real) >,
	func<vec1 (vec3)>,
	func<real (vec3, vec2)>
	>;
  
  virtual cast_type cast() = 0;

};

template<class To, class ... From>
struct func< To (From...) > : public func_base,
							  public std::enable_shared_from_this< func< To (From...) > >{
  
  virtual void apply(To& to, const From&... from) const = 0; 
  virtual std::size_t size(const From&... from) const = 0;
  
  func_base::cast_type cast() {
	return this->shared_from_this();
  }
  
};


template<int M, int N, class U = real>
struct sum : func< U (vector<M, U>, vector<N, U> ) > {

  virtual void apply(U& to, const vector<M, U>& lhs, const vector<N, U>& rhs) const {
	to = lhs.sum() + rhs.sum();
  }
  
  virtual std::size_t size(const vector<M, U>& lhs, const vector<N, U>& rhs) const {
	return 1;
  }
  
};



using vertex = variant<dofs_base, func_base>;
struct edge {};

struct graph : dependency_graph<vertex, edge> {

  template<class T, class ... Args>
  unsigned add_shared(Args&& ... args) {
	return add_vertex( std::make_shared<T>(std::forward<Args>(args)...), *this);
  }


  range<graph::vertex_iterator> vertices() {
	return make_range( boost::vertices(*this) );
  }
  
};


struct typecheck {

  void operator()(dofs_base* self, unsigned v, const graph& g) const {
	
  }

  // dispatch
  void operator()(func_base* self, unsigned v, const graph& g) const {
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

	// dispatch
	void operator()(dofs_base* self) const {
	  self->cast().apply(*this);
	}

	void operator()(func_base* self) const {
	  self->cast().apply(*this);
	}

	// ok cases
	void operator()(dofs<Expected>* ) const { };

	template<class ... From>
	void operator()(func<Expected (From...) >* ) const { };	


	// error cases
	template<class T>
	void operator()(dofs<T>* ) const {
	  throw std::runtime_error("type error: "+ demangle( typeid(Expected (T) ).name() ));
	}


	template<class T, class ... From>
	void operator()(func<T (From...) >* ) const {
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


  // typecheck from types
  template<class To, class ... From>
  void operator()(func<To (From...)>* self, unsigned v, const graph& g) const {

	auto out = adjacent_vertices(v, g);
	try {
	  const int expand[] = { (check_type<From>(out.first++, out.second, g), 0)...};
	  
	  if(out.first != out.second) {
		throw std::runtime_error("too many out edges");
	  }
	} catch( std::runtime_error& e ){
	  throw std::runtime_error(e.what() 
							   + std::string(" when processing vertex: ") 
							   + std::to_string(v) );
	}
	
  }
  
};


// apply itself on the same variant after calling ->cast()
template<class Derived>
struct dispatch {

  const Derived& derived() const { return static_cast<const Derived&>(*this); }

  // note: we don't want perfect forwarding here since it may preempt
  // derived classes operator()
  template<class T, class ... Args>
  void operator()(T* self, const Args& ... args) {
	self->cast().apply( derived(), args...);
  }
  
};


struct stack_overflow : std::runtime_error {
  const std::size_t required;
  stack_overflow(std::size_t required)
	: std::runtime_error("stack overflow"),
	  required(required) { }
};

struct frame_type {
  std::size_t start, size;		// TODO store dim (number of dofs)
};

struct propagate : dispatch<propagate> {

  std::vector<real>& stack;		// stack
  std::size_t& sp; 				// stack pointer
  std::vector<frame_type>& frame;   	// frame pointers
  

  propagate(std::vector<real>& stack,
			std::size_t& sp,
			std::vector<frame_type>& frame)
	: stack(stack),
	  sp(sp),
	  frame(frame) {

  }

  using dispatch<propagate>::operator();


  template<class G>
  std::size_t aligned_sp(std::size_t size) const {
	std::size_t space = -1;
	void* buf = stack.data() + sp;
	real* aligned = (real*)std::align( alignof(G), size * sizeof(real), buf, space);
	return aligned - stack.data();
  }

  template<class G>
  std::size_t allocate(std::size_t dim) const {
	assert( stack.capacity() );
	
	const std::size_t new_sp = aligned_sp<G>(dim);
	const std::size_t required = new_sp + dim;
	if( required > stack.capacity() ) throw stack_overflow(required);
	
	// allocate space
	stack.resize( required );

	return new_sp;
  }
  

  // push dofs data to the stack  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
	const std::size_t dim = traits<G>::size(self->pos);

	// allocate data
	sp = allocate<G>(dim);
	
	// copy
	G& data = reinterpret_cast<G&>(stack[sp]);
	data = self->pos;
	
	// set stack/frame pointers
	frame[v] = {sp, dim};
	
	sp += dim;
  }


  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g) const {
	operator()(self, v, g, indices_for<From...>() );
  }

  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g,
				  indices<I...> idx) const {

	frame_type pframes[sizeof...(From)];
	
	// fetch frame pointers
	{
	  frame_type* fp = pframes;
	  for(unsigned p : make_range(adjacent_vertices(v, g)) ) {
		*fp++ = frame[p];
	  }
	}

	// result size
	const std::size_t dim =
	  self->size(reinterpret_cast<const From&>(stack[ pframes[I].start ])...);

	// allocate
	sp = allocate<To>(dim);
	
	To& to = reinterpret_cast<To&>(stack[sp]);

	self->apply(to, reinterpret_cast<const From&>(stack[ pframes[I].start ])...);

	std::clog << "mapped: " << to << std::endl;
	
	// set stack/frame pointers
	frame[v] = {sp, dim};
	
	sp += dim;
  }
  
  
};






int main(int, char**) {

  graph g;

  auto point3 = std::make_shared< dofs<vec3> >();
  auto point2 = std::make_shared< dofs<vec2> >();  
  auto map = std::make_shared< sum<3, 2> >();

  point3->pos = {1, 2, 3};
  point2->pos = {4, 5};  
  
  unsigned u = add_vertex(point3, g);
  unsigned w = add_vertex(point2, g);
  unsigned v = add_vertex(map, g);
  
  add_edge(v, u, g);
  add_edge(v, w, g);
  
  std::vector<unsigned> order;
  g.sort(order);

  for(unsigned v : order) {
	g[v].apply( typecheck(), v, g );
  }
  
  std::vector<real> stack(1);
  std::size_t sp;
  std::vector<frame_type> frame(num_vertices(g));

  propagate vis(stack, sp, frame);
  
  while(true) {
	try{
	  sp = 0;
	  stack.clear();
	  for(unsigned v : order) {
		g[v].apply(vis, v, g);
	  }
	  break;
	} catch( stack_overflow& e ) {
	  stack.reserve(e.required);
	  std::clog << "stack reserve: " << stack.capacity() << std::endl;
	}
  }

  return 0;
}
 
