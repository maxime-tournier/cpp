#ifndef CORE_GRAPH_HPP
#define CORE_GRAPH_HPP

#include <map>
#include <memory>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>

#include <core/variant.hpp>
#include <core/range.hpp>

#include "dofs.hpp"
#include "func.hpp"
#include "metric.hpp"



using vertex = variant<dofs_base, func_base, metric_base>;
struct edge {};


template<class Vertex>
struct vertex_property : Vertex {

  using Vertex::Vertex;

  vertex_property(const Vertex& vertex) : Vertex(vertex) { }
  vertex_property() { }
  
  boost::default_color_type color;
  unsigned char flags = 0;
  
  template<unsigned char c>
  void set() { flags |= c; }
  
};


class graph : public boost::adjacency_list<boost::vecS,
										   boost::vecS,
										   boost::bidirectionalS,
										   vertex_property<vertex>,
										   edge> {

  
  std::map< void*, unsigned > table;

  struct get_vertex {
	graph* owner;

	void operator()(void* ptr, unsigned& res) const {
	  auto it = owner->table.find(ptr);
	  if(it == owner->table.end()) throw std::runtime_error("unknown vertex");
	  res = it->second;
	}
	
  };
  
public:

  enum {
	dofs_type,
	func_type,
	metric_type,
  };


  template<class Iterator>
  void sort(Iterator out) {
	topological_sort(*this, out, color_map(get(&vertex_property<vertex>::color, *this)));
  }

  
  template<class T, class ... Args>
  std::shared_ptr< T > add_shared(Args&& ... args) {
	
  	auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
  	add(ptr);
	
  	return ptr;
  }


  void add(const vertex& ptr) {
	ptr.apply([&](void* addr) {
		table[addr] = add_vertex(ptr, *this);
	  });
  }



  void connect(const vertex& src, const vertex& dst) {
	unsigned s, d;
	
	src.apply(get_vertex{this}, s);
	dst.apply(get_vertex{this}, d);

	add_edge(s, d, *this);
  }


  void disconnect(const vertex& src, const vertex& dst) {
	unsigned s, d;
	
	src.apply(get_vertex{this}, s);
	dst.apply(get_vertex{this}, d);
	auto e = boost::edge(s, d, *this);

	if(!e.second) throw std::runtime_error("unknown edge");
	
	remove_edge(e.first);
  }
  
  
  range<graph::vertex_iterator> vertices() {
    return make_range( boost::vertices(*this) );
  }

  
};



#endif
