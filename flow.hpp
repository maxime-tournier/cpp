#ifndef FLOW_HPP
#define FLOW_HPP

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/topological_sort.hpp>

#include <iostream>

#include "range.hpp"

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


template<class Vertex, class Edge>
struct dependency_graph : boost::adjacency_list<boost::vecS,
												boost::vecS,
												boost::bidirectionalS,
												vertex_property<Vertex>,
												Edge> {
  enum {
	clear = 0,
	dirty = 1,
	needed = 2
  };

  
  void sort(std::vector<unsigned>& out) {
	out.clear();
	topological_sort(*this, std::back_inserter(out), 
					 color_map(get(&vertex_property<Vertex>::color, *this)));
  }
  

  template<class F>
  void update(std::vector<unsigned>& order, const F& f) {
	
	// propagate needed, sources first
	for(unsigned v : reverse(order)) {
	  auto& vp = (*this)[v];
	  
	  const bool is_needed = vp.flags & needed;
	  if(!is_needed) continue;
	  
	  for(auto out = out_edges(v, *this); out.first != out.second; ++out.first) {
		const unsigned dst = target(*out.first, *this);
		(*this)[dst].flags |= needed;
	  }
	  

	}

	// propagate dirty, sinks first
	for(unsigned v : order) {
	  auto& vp = (*this)[v];
	  
	  const bool is_dirty =  vp.flags & dirty;
	  
	  if( !is_dirty ) continue;

	  const bool is_needed = vp.flags & needed;
	  if( is_needed ) {
		f(v);
	  }
	  
	  for(auto in = in_edges(v, *this); in.first != in.second; ++in.first) {
		const unsigned src = source(*in.first, *this);
		(*this)[src].flags |= dirty;
	  }

	}

	
  }

 

};





#endif
