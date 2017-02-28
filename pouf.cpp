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

  static const std::size_t deriv_dim = N * traits<U>::deriv_dim;
  static const std::size_t coord_dim = N * traits<U>::coord_dim;
  
};


template<>
struct traits<real> {
  static const std::size_t deriv_dim = 1;
  static const std::size_t coord_dim = 1;
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

  std::size_t size() const {
    return 1;
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

using triplet = Eigen::Triplet<real>;

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
struct numbering : dispatch<numbering> {

  struct chunk {
    std::size_t start, size;
  };

  numbering(graph_data& storage, std::size_t& offset, const graph_data& pos)
    : storage(storage),
      offset(offset),
      pos(pos) {

  }

  using dispatch::operator();
  
  graph_data& storage;
  std::size_t& offset;
  const graph_data& pos;
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v) const {
    chunk* c = storage.allocate<chunk>(v);

    c->size = pos.count(v) * traits<G>::deriv_dim;
    c->start = offset;
    
    offset += c->size;
  }


  template<class To, class ... From>
  void operator()(func<To (From...) >* self, unsigned v) const {
    chunk* c = storage.allocate<chunk>(v);
    
    c->size = pos.count(v) * traits<To>::deriv_dim;
    c->start = offset;
    
    offset += c->size;
  }  
  
};




using rmat = Eigen::SparseMatrix<real, Eigen::RowMajor>;

// fetch jacobians w/ masking
struct fetch : dispatch<fetch> {

  using jacobian_type = std::vector<triplet>;
  jacobian_type& jacobian;
  graph_data& mask;
  
  using elements_type = std::vector< jacobian_type >;
  
  // internal
  elements_type& elements;

  const graph_data& chunks;
  const graph_data& pos;
  
  fetch(jacobian_type& jacobian,
	graph_data& mask,
	elements_type& elements,
	const graph_data& chunks,	
	const graph_data& pos)
    : jacobian(jacobian),
      mask(mask),
      elements(elements),
      chunks(chunks),
      pos(pos) {
    
  }

  using dispatch<fetch>::operator();
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
    const numbering::chunk& chunk = chunks.get<numbering::chunk>(v);

    for(unsigned i = chunk.start, n = chunk.start + chunk.size; i < n; ++i) {
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
      
    const std::size_t rows = count * traits<To>::deriv_dim;
    const std::size_t cols[] = { parent_count[I] * traits<From>::deriv_dim... };    
    
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

    const numbering::chunk& chunk = chunks.get<numbering::chunk>(v);
    
    for(unsigned p = 0; p < n; ++p) {
      const unsigned vp = parents[p];
      const numbering::chunk& parent_chunk = chunks.get<numbering::chunk>(vp);
	
      // shift elements
      for(triplet& it : elements[p]) {
	// TODO filter by mask
	jacobian.emplace_back(it.row() + chunk.start,
			      it.col() + parent_chunk.start,
			      it.value());
      }
    }

  }
  
};


struct assemble : dispatch<assemble> {
  graph_data& offset;
  std::size_t& current;
  
  std::vector<rmat>& jacobian;
  std::vector<std::vector<rmat>>& blocks;

  
  using dispatch<assemble>::operator();
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
    const std::size_t count = self->size();

    std::size_t& off = offset.allocate<std::size_t>(v);
    off = current;

    const std::size_t rows = count * traits<G>::deriv_dim;

    // TODO 
    
    current += rows;
  }
    
  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g) const {
    operator()(self, v, g, indices_for<From...>() );
  }

  
  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g,
		  indices<I...> idx) const {
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

  const std::size_t n = num_vertices(g);
  
  graph_data pos(n);
  graph_data chunks(n);

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
  
  std::vector<triplet> triplets;
  std::vector< std::vector<triplet> > elements;
  
  // compute masks/jacobians
  with_auto_stack([&] {
      for(unsigned v : reverse(order)) {
	g[v].apply( fetch(triplets, mask, elements, chunks, pos), v, g);
      }
    });

  rmat jacobian(total_dim, total_dim);
  jacobian.setFromTriplets(triplets.begin(), triplets.end());

  std::cout << jacobian << std::endl;
  
  return 0;
}
 
