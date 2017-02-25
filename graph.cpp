
#include <boost/graph/adjacency_list.hpp>

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/breadth_first_search.hpp>

#include <iostream>


template<class Vertex, class Edge>
struct graph : boost::adjacency_list<boost::vecS,
												boost::vecS,
												boost::directedS,
												Vertex,
												Edge> {
  
};



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


class bfs_visitor : public boost::default_bfs_visitor {

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
	boost::default_color_type color;
  };
  
  struct edge {};  
  graph<vertex, edge> g;

  unsigned u = add_vertex(g);  
  unsigned v = add_vertex(g);
  unsigned w = add_vertex(g);
  unsigned x = add_vertex(g);    

  add_edge(u, v, g);
  add_edge(v, w, g);
  add_edge(v, x, g);    
  
  vertex vp = g[v];

  std::cout << "dfs"<< std::endl;
  dfs_visitor dfs;
  depth_first_search(g, visitor(dfs).color_map(get(&vertex::color, g)));

  std::cout << "bfs"<< std::endl;  
  bfs_visitor bfs;
  unsigned start = u;
  breadth_first_search(g, start, visitor(bfs).color_map(get(&vertex::color, g)));
  
}
