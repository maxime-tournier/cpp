#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <map>
#include <memory>

#include "../flow.hpp"
#include "../variant.hpp"

// struct dofs_base;
// struct func_base;
// struct metric_base;

#include "dofs.hpp"
#include "func.hpp"
#include "metric.hpp"


using vertex = variant<dofs_base, func_base, metric_base>;
struct edge {};

class graph : public dependency_graph<vertex, edge> {


  
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
