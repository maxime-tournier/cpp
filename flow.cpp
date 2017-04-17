#include "flow.hpp"

#include <boost/graph/adjacency_list.hpp>

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/breadth_first_search.hpp>

#include <iostream>



class dfs_visitor : public boost::default_dfs_visitor {

public:

  template < typename Vertex, typename Graph >
  void discover_vertex(Vertex u, const Graph & g) const {
	std::cout << "disc:\t" << u << std::endl;
  }

  template < typename Vertex, typename Graph >
  void initialize_vertex(Vertex u, const Graph & g) const {
	std::cout << "init:\t" << u << std::endl;
  }

  
  template < typename Vertex, typename Graph >
    void finish_vertex(Vertex u, const Graph & g) const {
	std::cout << "finish:\t" << u << std::endl;	
  }

  template < typename Vertex, typename Graph >
  void start_vertex(Vertex u, const Graph & g) const {
	std::cout << "start:\t" << u << std::endl;	
  }


  template < typename Edge, typename Graph >
  void tree_edge(Edge e, const Graph & g) const {
	std::cout << "tree:\t" << e << std::endl;	
  }


  template < typename Edge, typename Graph >
  void back_edge(Edge e, const Graph & g) const {
	std::cout << "back:\t" << e << std::endl;	
  }
  

  
  
};




int main(int, char** ) {


  struct vertex {
	std::string name;
  };
  
  struct edge {};
  
  using graph = dependency_graph<vertex, edge>;
  graph g;
  
  unsigned u = add_vertex(g);  
  unsigned v = add_vertex(g);
  unsigned w = add_vertex(g);
  unsigned x = add_vertex(g);    

  add_edge(u, v, g);
  add_edge(v, w, g);
  add_edge(v, x, g);    
  
  std::vector<unsigned> order;
  g.sort(std::back_inserter(order));

  g[x].set<graph::dirty>();
  g[v].set<graph::needed>();
  
  g.update(order, [&](unsigned v) {
	  std::cout << "updated " << v << std::endl;
	  g[v].set<graph::clear>();
	});

}
