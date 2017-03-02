#include "flow.hpp"
#include "variant.hpp"
#include "indices.hpp"

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <cassert>

#include <cstdlib>
#include <memory>
#include <cxxabi.h>

std::string demangle(const char* name) {

  int status;

  // enable c++11 by passing the flag -std=c++11 to g++
  std::unique_ptr<char, void(*)(void*)> res = {
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

template<class G>
using deriv = typename traits<G>::deriv;

template<class G>
using scalar = typename traits<G>::scalar;


template<int N, class U>
struct traits< vector<N, U> > {

  using scalar = typename traits<U>::scalar;
  using deriv = vector<N, typename traits<U>::deriv>;
  
  static const std::size_t dim = N * traits<U>::dim;

  static scalar dot(const vector<N, U>& x,
					const vector<N, U>& y) {
    return x.dot(y);
  }

  static scalar& coord(std::size_t i, vector<N, U>& v) {
    return reinterpret_cast<scalar*>(v.data())[i];
  }

  static const scalar& coord(std::size_t i, const vector<N, U>& v) {
    return reinterpret_cast<scalar*>(v.data())[i];
  }
  
};



template<>
struct traits<real> {
  
  using deriv = real;
  using scalar = real;

  static const std::size_t dim = 1;

  static scalar dot(real x, real y) { return x * y; }

  static const scalar& coord(std::size_t, const real& value) { return value; }
  static scalar& coord(std::size_t, real& value) { return value; }  
};


using triplet = Eigen::Triplet<real>;

// TODO maybe we can simplify stiffness based on mappings ?

template<class G>
struct metric;

enum class metric_kind {
  mass,
  damping,
  stiffness
};

struct metric_base {
  virtual ~metric_base() { }

  using cast_type = variant< 
    metric<real>,
    metric<vec1>,
    metric<vec2>,
    metric<vec3>,
    metric<vec4>,
    metric<vec6>>;

  virtual cast_type cast() = 0;

  const metric_kind kind;
  
  metric_base(metric_kind kind) : kind(kind) { };
};

template<class G>
struct metric : metric_base, std::enable_shared_from_this< metric<G> > {

  using metric_base::metric_base;
  
  typename metric::cast_type cast() { return this->shared_from_this(); }
  virtual void tensor(std::vector<triplet>& out, const G& at) const = 0;

};


template<metric_kind kind, class G>
struct uniform : metric<G> {

  uniform(real value = 1.0)
	: metric<G>(kind),
	value(value) {
	
  }
  
  real value;
  
  virtual void tensor(std::vector<triplet>& out, const G& at) const {
	for(unsigned i = 0, n = traits<G>::dim; i < n; ++i) {
	  out.emplace_back(i, i, value);
	}
  }
	
};



template<class T>
struct constraint;

struct constraint_base {
  virtual ~constraint_base() { }

  using cast_type = variant<constraint<real>>;
  virtual cast_type cast() = 0;
};



struct dofs_base {
  virtual ~dofs_base() { }

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

  std::size_t size() const {
    return 1;
  }
  
};

template<class T>
struct func;

struct func_base {
  virtual ~func_base() { }

  using cast_type = variant< 
    func<real (real) >,
    func<vec1 (vec3)>,
    func<real (vec3, vec2)>
    >;
  
  virtual cast_type cast() = 0;

};



template<class, class T>
using repeat = T;

template<class To, class ... From>
struct func< To (From...) > : public func_base,
							  public std::enable_shared_from_this< func< To (From...) > >{

  func_base::cast_type cast() {
    return this->shared_from_this();
  }
  
  // output size from inputs
  virtual std::size_t size(const From&... from) const = 0;

  // apply function
  virtual void apply(To& to, const From&... from) const = 0;

  // sparse jacobian
  virtual void jacobian(repeat<From, std::vector<triplet>>& ... out,
						const From& ... from) const = 0;
  
  
};


template<int M, int N, class U = real>
struct sum : func< U (vector<M, U>, vector<N, U> ) > {

  virtual std::size_t size(const vector<M, U>& lhs, const vector<N, U>& rhs) const {
    return 1;
  }
  
  
  virtual void apply(U& to, const vector<M, U>& lhs, const vector<N, U>& rhs) const {
    to = lhs.sum() + rhs.sum();
  }
  

  virtual void jacobian(std::vector<triplet>& lhs_block, std::vector<triplet>& rhs_block,			
						const vector<M, U>& lhs, const vector<N, U>& rhs) const {

    for(unsigned i = 0, n = lhs.size(); i < n; ++i) {
      lhs_block.emplace_back(0, i, 1.0);
    }
    
    for(unsigned i = 0, n = rhs.size(); i < n; ++i) {
      rhs_block.emplace_back(0, i, 1.0);      
    }

  }
  
};



template<class U>
struct norm2 : func< typename traits<U>::scalar ( U ) > {
  
  virtual std::size_t size(const U& ) const {
    return 1;
  }
  
  
  virtual void apply(typename traits<U>::scalar& to, const U& from) const {
    to = traits<U>::dot(from, from) / 2.0;
  }
  

  virtual void jacobian(std::vector<triplet>& block, const U& from) const {
    for(unsigned i = 0, n = traits< deriv<U> >::dim; i < n; ++i) {
      block.emplace_back(0, i, traits<U>::coord(i, from));
    }
    
  }
  
};





using vertex = variant<dofs_base, func_base, metric_base>;
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
	if( out_degree(v, g) > 0 ) {
	  throw std::runtime_error("dofs must be independent (zero out-edge)");
	}
  }

  void operator()(metric_base* self, unsigned v, const graph& g) const {

	if( out_degree(v, g) != 1 ) {
	  throw std::runtime_error("metric must be dependent (single out-edge)");
	}

	self->cast().apply(*this, v, g);
  }


  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {

	auto p = *adjacent_vertices(v, g).first;
	
	g[p].apply(expected_check<G>());
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

	template<class T>
	void operator()(T* self) const {
	  throw std::logic_error("should not happen");
	}
	
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
      throw std::runtime_error("expected more out-edges");
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

      // TODO this could be ok though
      if(out.first != out.second) {
		throw std::runtime_error("expected less out-edges");
      }
    } catch( std::runtime_error& e ){
      throw std::runtime_error(e.what() 
							   + std::string(" for vertex: ") 
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





class stack {
  std::vector<char> storage;
  std::size_t sp;

  // TODO not sure we need num here
  template<class G>
  std::size_t aligned_sp(std::size_t num) {
    std::size_t space = -1;
    void* buf = storage.data() + sp;
    char* aligned = (char*)std::align( alignof(G), num * sizeof(G), buf, space);
    return aligned - storage.data();
  }


public:

  struct frame {
    // stack pointer, buffer size, element count
    std::size_t sp, size, count;
  };
  
  struct overflow : std::runtime_error {
    stack& who;
    const std::size_t required;
    
    overflow(stack& who, std::size_t required)
      : std::runtime_error("stack overflow"),
	  who(who),
	  required(required) { }
    
  };
  
  
  stack(std::size_t size = 0)
    : storage(size),
      sp(0) {
    
  }
  
  template<class G>
  G* allocate(frame& f, std::size_t count = 1) {
    assert(count > 0 && "zero-size allocation");
    
    const std::size_t size = count * sizeof(G);

    if(!storage.capacity()) {
      reserve(size);
    }
    
    // align
    sp = aligned_sp<G>(count);
    const std::size_t required = sp + size;
    
    if( required > storage.capacity() ) {
      throw overflow(*this, required);
    }
    
    // allocate space
    storage.resize( required );

    G* res = reinterpret_cast<G*>(&storage[sp]);
    
    // record frame
    f = {sp, size, count};
    sp += size;
	
    return res;
  }


  template<class G>
  G& get(const frame& f) {
    return reinterpret_cast<G&>(storage[f.sp]);
  }

  template<class G>
  const G& get(const frame& f) const {
    return reinterpret_cast<const G&>(storage[f.sp]);
  }
  

  void reset() {
    sp = 0;
  }


  std::size_t capacity() const { return storage.capacity(); }
  std::size_t size() const { return storage.size(); }

  void reserve(std::size_t size) {
    std::clog << "stack reserve: " << capacity()
			  << " -> " << size << std::endl;
	  
    storage.reserve(size);
  }

  void grow(std::size_t required = 0) {
    const std::size_t c = capacity();
    assert( c );

    const std::size_t min = std::max(required, c+1);
    reserve( std::max(min, c + c / 2) );
  }
};


// TODO we probably want a tag phantom type here to get stricter
// typechecking
class graph_data {
  stack storage;
  std::vector<stack::frame> frame;
public:
  graph_data(std::size_t num_vertices,
			 std::size_t capacity = 0)
    : storage(capacity),
      frame(num_vertices) {

  }

  
  template<class G>
  G* allocate(unsigned v, std::size_t count = 1) {
    return storage.allocate<G>(frame[v], count);
  }
  

  template<class G>
  G& get(unsigned v) {
    return storage.get<G>(frame[v]);
  }

  template<class G>
  const G& get(unsigned v) const {
    return storage.get<G>(frame[v]);    
  }


  std::size_t count(unsigned v) const {
    return frame[v].count;
  }


  // TODO reserve, size, etc
  void grow() {
    storage.grow();
  }

  void reset() {
    storage.reset();
  }
  
};




// push positions
struct push : dispatch<push> {

  graph_data& pos;
  
  push(graph_data& pos) : pos(pos) { }
  
  
  using dispatch::operator();

  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {

  }
  
  
  // push dofs data to the stack  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
    const std::size_t count = self->size();

    // allocate
    G* data = pos.allocate<G>(v, count);
    
    // TODO static_assert G is pod ?
    
    // copy
    *data = self->pos;
  }


  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g) const {
    operator()(self, v, g, indices_for<From...>() );
  }


  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g,
				  indices<I...> idx) const {

    // note: we need random access parent iterator for this
    auto parents = adjacent_vertices(v, g).first;
    
    // result size
    const std::size_t count =
      self->size( pos.get<const From>(parents[I])... );
    
    // allocate result
    To* to = pos.allocate<To>(v, count);
    
    self->apply(*to, pos.get<const From>(parents[I])...);
    
    std::clog << "mapped: " << *to << std::endl;
  }
  
  
};


// deriv data numbering
struct chunk {
  std::size_t start, size;
};

struct numbering : dispatch<numbering> {

  using chunks_type = std::vector<chunk>;
  numbering(chunks_type& chunks,
			std::size_t& offset, 
			const graph_data& pos)
    : chunks(chunks),
      offset(offset),
      pos(pos) {

  }

  using dispatch::operator();
  
  chunks_type& chunks;
  std::size_t& offset;
  const graph_data& pos;
  
  template<class G>
  void operator()(metric<G>* self, unsigned v) const {
	// TODO don't dispatch
  }
  
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v) const {
	chunk& c = chunks[v];
	
    c.size = pos.count(v) * traits< deriv<G> >::dim;
    c.start = offset;
    
    offset += c.size;
  }


  template<class To, class ... From>
  void operator()(func<To (From...) >* self, unsigned v) const {
    chunk& c = chunks[v];
	
    c.size = pos.count(v) * traits< deriv<To> >::dim;
    c.start = offset;
    
    offset += c.size;
  }  
  
};




using rmat = Eigen::SparseMatrix<real, Eigen::RowMajor>;

// fetch jacobians w/ masking
struct fetch : dispatch<fetch> {

  using matrix_type = std::vector<triplet>;
  matrix_type& jacobian;
  matrix_type& diagonal;
  
  graph_data& mask;
  
  using elements_type = std::vector< matrix_type >;
  
  // internal
  elements_type& elements;

  const numbering::chunks_type& chunks;
  const graph_data& pos;
  
  fetch(matrix_type& jacobian,
		matrix_type& diagonal,
		graph_data& mask,
		elements_type& elements,
		const numbering::chunks_type& chunks,	
		const graph_data& pos)
    : jacobian(jacobian),
	  diagonal(diagonal),
      mask(mask),
      elements(elements),
      chunks(chunks),
      pos(pos) {
    
  }

  real dt = 1.0;
  
  using dispatch::operator();

  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {

	// remember current size
	const std::size_t start = diagonal.size();

	// obtain triplets
	self->tensor(diagonal, pos.get<G>(v));
	
	const std::size_t end = diagonal.size();

	real factor = 0;

	switch(self->kind) {
	case metric_kind::mass: factor = 1; break;
	case metric_kind::damping: factor = dt; break;
	case metric_kind::stiffness: factor = dt * dt; break;	  
	};

	const unsigned parent = *adjacent_vertices(v, g).first;
	
	const chunk& c = chunks[parent];

	// shift/scale inserted data
	for(unsigned i = start; i < end; ++i) {
	  auto& it = diagonal[i];
	  it = {it.row() + int(c.start),
			it.col() + int(c.start),
			factor * it.value()};
	}
	
  }

  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
    const chunk& c = chunks[v];

    for(unsigned i = c.start, n = c.start + c.size; i < n; ++i) {
      jacobian.emplace_back(i, i, 1.0);
    }
  }
    
  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g) const {
    operator()(self, v, g, indices_for<From...>() );
  }


  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g,
				  indices<I...> idx) const {

    auto parents = adjacent_vertices(v, g).first;

    // get dofs count
    const std::size_t count = pos.count(v);
    const std::size_t parent_count[] = { pos.count(parents[I])... };
      
    const std::size_t rows = count * traits< deriv<To> >::dim;
    const std::size_t cols[] = { parent_count[I] * traits< deriv<From> >::dim... };    
    
    // no child: allocate mask
    if( in_degree(v, g) == 0 ) {
      mask.allocate<char>(v, count);
    }

    const std::size_t n = sizeof...(From);
    
    if( elements.size() < n) elements.resize(n);
    
    for(unsigned p = 0; p < n; ++p) {
      // alloc parent masks
      const unsigned vp = parents[p];
      mask.allocate<char>(vp, parent_count[p]);

      // TODO make sure this does not dealloc
      elements[p].clear();
    }

    // fetch data
    elements.clear();
    self->jacobian(elements[I]..., pos.get<const From>(parents[I])...);
    
    // TODO set parent masks
    const chunk& curr_chunk = chunks[v];
    
    for(unsigned p = 0; p < n; ++p) {
      const unsigned vp = parents[p];
      const chunk& parent_chunk = chunks[vp];
	
      // shift elements
      for(triplet& it : elements[p]) {
		// TODO filter by mask
		jacobian.emplace_back(it.row() + curr_chunk.start,
							  it.col() + parent_chunk.start,
							  it.value());
      }
    }

  }
  
};


struct concatenate {
  rmat& jacobian;
  const numbering::chunks_type& chunks;

  concatenate(rmat& jacobian,
			  const numbering::chunks_type& chunks)
    : jacobian(jacobian),
      chunks(chunks) {

  }
  
  void operator()(dofs_base* self, unsigned v) const { }
  
  void operator()(func_base* self, unsigned v) const {
    const chunk& c = chunks[v];

    // noalias should be safe here (diagonal is zero)
    jacobian.middleRows(c.start, c.size) =
      jacobian.middleRows(c.start, c.size) * jacobian;
    
  }

  

  
};



template<class F>
static void with_auto_stack( const F& f ) {
  while (true) {
    try{
      f();
      return;
    } catch(stack::overflow& e) {
      e.who.grow();
      e.who.reset();
    }
  }
}


static void concatenate(rmat& res, const rmat& src) {
  res.resize(src.rows(), src.cols());

  std::vector<bool> mask(src.cols());
  std::vector<real> values(src.cols());
  std::vector<int> indices(src.cols());  
  
  // TODO better estimate? 
  const int estimated_nnz = src.nonZeros() + src.nonZeros() / 2;
  res.reserve(estimated_nnz);
  
  std::fill(mask.begin(), mask.end(), false);
  
  for(unsigned i = 0, n = src.rows(); i < n; ++i) {
	unsigned nnz = 0;
	
	for(rmat::InnerIterator src_it(src, i); src_it; ++src_it) {
	  const real y = src_it.value();
	  const unsigned k = src_it.col();

	  // take row from previously computed if possible
	  for(rmat::InnerIterator res_it(k < i ? res : src, k); res_it; ++res_it) {
		const unsigned j = res_it.col();
        const real x = res_it.value();

		if(!mask[j]) {
		  mask[j] = true;
		  values[j] = x * y;
		  indices[nnz] = j;
          ++nnz;
		} else {
		  values[j] += x * y;
		}
	  }
	}

	res.startVec(i);
	for(unsigned k = 0; k < nnz; ++k) {
	  const unsigned j = indices[k];
	  res.insertBack(i, j) = values[j];
	  mask[j] = false;
	}
	
  }
  
  res.finalize();
}


int main(int, char**) {

  graph g;

  // independent dofs
  auto point3 = std::make_shared< dofs<vec3> >();
  auto point2 = std::make_shared< dofs<vec2> >();

  auto mass3 = std::make_shared< uniform<metric_kind::mass, vec3> >(2);
  auto mass2 = std::make_shared< uniform<metric_kind::mass, vec2> >();  
  
  // mapped
  auto map = std::make_shared< sum<3, 2> >();
  auto map2 = std::make_shared< norm2<double> >();  

  auto ff1 = std::make_shared< uniform<metric_kind::stiffness, double> >(3);
  
  point3->pos = {1, 2, 3};
  point2->pos = {4, 5};  
  
  unsigned p3 = add_vertex(point3, g);
  unsigned p2 = add_vertex(point2, g);

  
  unsigned f3 = add_vertex(map, g);
  add_edge(f3, p3, g);
  add_edge(f3, p2, g);

  unsigned f2 = add_vertex(map2, g);  
  add_edge(f2, f3, g);  
  
  
  unsigned m3 = add_vertex(mass3, g);
  add_edge(m3, p3, g);

  unsigned m2 = add_vertex(mass2, g);
  add_edge(m2, p2, g);

  unsigned k1 = add_vertex(ff1, g);
  add_edge(k1, f2, g);
  
  std::vector<unsigned> order;
  g.sort(order);

  for(unsigned v : order) {
    g[v].apply( typecheck(), v, g );
  }

  const std::size_t n = num_vertices(g);
  
  graph_data pos(n);
  std::vector<chunk> chunks(n);

  // propagate positions and number dofs
  std::size_t offset;
  
  with_auto_stack([&] {
      offset = 0;
      for(unsigned v : order) {
		g[v].apply( push(pos), v, g);
		g[v].apply( numbering(chunks, offset, pos), v);	
      }
    });

  const std::size_t total_dim = offset;
  
  graph_data mask(n);
  
  std::vector<triplet> jacobian, diagonal;
  std::vector< std::vector<triplet> > elements;

  const real dt = 0.01;
  
  // compute masks/jacobians
  with_auto_stack([&] {
      jacobian.clear();
      diagonal.clear();

	  fetch vis(jacobian, diagonal, mask, elements, chunks, pos);
	  vis.dt = dt;
	  
      for(unsigned v : reverse(order)) {
		g[v].apply( vis, v, g);
      }
    });

  rmat J(total_dim, total_dim), H(total_dim, total_dim);
  
  J.setFromTriplets(jacobian.begin(), jacobian.end());
  H.setFromTriplets(diagonal.begin(), diagonal.end());  

  std::cout << "diagonal: " << H << std::endl;
  
  rmat concat;
  concatenate(concat, J);
  
  std::cout << "after concatenation: " << std::endl;
  std::cout << concat << std::endl;

  rmat full = concat.transpose() * H * concat;
  std::cout << "H: " << full << std::endl;
  
  return 0;
}
 
