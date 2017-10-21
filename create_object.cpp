// -*- compile-command: "/usr/bin/g++ -std=c++11 -Wall -g create_object.cpp -o create_object " -*-

#include <vector>
#include <memory>
#include <map>
#include <sstream>
#include <functional>
#include <typeindex>
#include <iostream>
#include <typeinfo>

template<class T>
struct traits;

template<class T>
using sptr = std::shared_ptr<T>;

static std::string quote(const std::string& value, char q = '"') {
  return q + value + q;
}


// type-safe attribute map
class attributes {
  using items_type = std::map<std::string, std::function< void() > >;
  items_type items;

  template<class T> struct boxed {
    const T value;
  };
  
public:
  
  template<class T>
  attributes& operator()(const std::string& name, T value) {
    items[name] = [value] {
      // std::clog << "throwing as " << traits<T>::name() << std::endl;
      throw boxed<T>{value};
    };
    return *this;
  }
  
  template<class T>
  const T& get(const std::string& name) const {
    // std::clog << "catching as " << traits<T>::name() << std::endl;    
    try {
      items.at(name)();
    } catch( const boxed<T>& payload ) {
      // std::clog << "success" << std::endl;
      return payload.value;
    } catch( std::out_of_range ) {
      throw;
    } catch( ... ) {
      throw std::bad_cast();
    }
    throw std::logic_error("should throw");
  }


  // convenience
  template<class T>
  attributes(const std::string& name, T value) { operator()(name, value); }

  attributes() { }
};

struct node;
struct object;


template<class T>
struct template_name {
  static std::string get() { return {}; }
};

template<template<class ...> class T, class ... Args>
struct template_name< T<Args...> > {
  
  static std::string get() {
    std::stringstream ss;
    
    // build template string
    (void) std::initializer_list<int> { (ss << traits<Args>::name() << ',', 0)... };
    
    // pop first comma
    std::string res = ss.str();
    res.pop_back();
    return res;
  }
};





// type info
struct info_type {

  // constructor function
  using ctor_type = std::shared_ptr<object>(*)( const node& ctx );
  ctor_type ctor;

  // data infos
  struct data_type {
    virtual ~data_type() { }
    
    const std::string name;
    const std::string doc;

    data_type(const std::string& name, const std::string& doc)
      : name(name), doc(doc) { }

    // set data value from attributes
    virtual void setup(object* obj, const attributes& attrs) const = 0;

    // throw a pointer to value
    virtual void get(object* obj) const = 0;
    
  };

  // data_name -> data
  std::map<std::string, std::shared_ptr<data_type> > data;
  
  // global tables
  using table_type = std::map< std::type_index, info_type>;
  static table_type table;

  // class_name -> template_name -> type_index
  using name_type = std::map< std::string, std::map<std::string, std::type_index> >;
  static name_type name;
};

// class_name -> template_name -> info
info_type::table_type info_type::table;
info_type::name_type info_type::name;



// register type infos
template<class T, class ... Bases>
class declare : public info_type {

  // inheritance
  template<class U, typename = typename std::enable_if< std::is_base_of<U, T>::value >::type>
  declare& inherit() {
    info_type& base = info_type::table.at(typeid(U));
    std::copy(base.data.begin(), base.data.end(), std::inserter(this->data, this->data.end()));
    return *this;
  }

  const std::string name;
public:
  
  declare(std::string name) : name(name) {
    this->ctor = [](const node& ctx) -> std::shared_ptr<object> {
      return std::make_shared<T>(ctx);
    };

    (void) std::initializer_list<int> { (inherit<Bases>(), 0)... };
  }

  ~declare() {
    // register ourselves to the global tables
    info_type::name[name].emplace(template_name<T>::get(), typeid(T));
    info_type::table.emplace(typeid(T), *this);
  }

  
  // data declaration
  template<class U, class V = U>
  declare& operator()(U (T::*member), std::string name, std::string doc) {

    // a typed data info
    struct typed_data_type : info_type::data_type {
      using member_type = U (T::*);
      const member_type member;

      typed_data_type(member_type member, const std::string& name, const std::string& doc) :
        info_type::data_type(name, doc),
        member(member) {
        
      }

      
      void get(object* obj) const {
        U* data = &(static_cast<T*>(obj)->*member);
        throw data;             // TODO do we need rtti?
      }

      void setup(object* obj, const attributes& attrs) const {
        U& data = (static_cast<T*>(obj)->*member);
        
        try {
          data = attrs.get<U>(name);
        } catch(std::bad_cast) {
          // wrong type: try deserialization from string
          std::stringstream ss;
        
          try{ 
            ss << attrs.get<std::string>(name);
          } catch(std::bad_cast) {
            try {
              ss << attrs.get<const char*>(name);
            } catch(std::bad_cast) {
              std::stringstream ss;
              ss << "cannot init data " << quote(name)
                 << " from attributes (types don't match, deserialization failed)";
              // TODO output value
              throw std::runtime_error(ss.str());
            }
          }
        
          data = traits<U>::parse(ss);
        
        } catch(std::out_of_range ) {
          // unspecified: keep it default-constructed
        } 
      }
    };

    // insert data
    if( !this->data.emplace(name, std::make_shared<typed_data_type>(member, name, doc) ).second ) {
      throw std::logic_error("duplicate data: " + name);
    }
    
    return *this;
  }

};



// an object may have data
struct object {
  virtual ~object() { }

  template<class T>
  T* data(const std::string& name) {
    try{ 
      const info_type& info = info_type::table.at(typeid(*this));

      try {
        info.data.at(name)->get(this);
      } catch(T* ptr) {
        return ptr;
      } catch( std::out_of_range ) {
        throw std::runtime_error("unknown data: " + name);
      } catch( ... ) {
        throw std::bad_cast();
      }
      
    } catch( std::out_of_range ) {
      throw std::runtime_error("unknown object type");
    }

    throw std::logic_error("should throw");
  }

};




// scene graph node
struct node {
  std::vector< std::shared_ptr<object> > objects;
  virtual ~node() { }

  std::shared_ptr<object> create(const std::string& class_name, const attributes& attrs);  
};


// create object
std::shared_ptr<object> node::create(const std::string& class_name, const attributes& attrs) {
  
  std::string template_name;

  {
    // fetch template name, if any
    static const char* template_key = "template";
    
    try {
      template_name = attrs.get<std::string>(template_key);
    } catch( std::bad_cast ) {
      // retry
      template_name = attrs.get<const char*>(template_key);
    } catch( std::out_of_range ) {
      // no template specified
    }
  }

  try{
    const auto& class_table = info_type::name.at(class_name);
    try {
      const info_type& info = info_type::table.at( class_table.at(template_name) );

      // construct object
      if( std::shared_ptr<object> res = info.ctor(*this) ) {

        // setup object data
        for(const auto& d : info.data) { d.second->setup(res.get(), attrs); }

        // insert object
        objects.emplace_back(res);
        return res;
        
      } else {
        throw std::logic_error("ctor returned null object");
      }
    } catch( std::out_of_range ) {
      std::stringstream ss;
      ss << "unknown template: " << class_name << '<' << template_name << '>';
      throw std::runtime_error(ss.str());
    }
  } catch( std::out_of_range ) {
    std::stringstream ss;
    ss << "unknown class: " << class_name;
    throw std::runtime_error(ss.str());    
  }
  
};


// some types + traits
using real = double;
struct vec3 {
  real x, y, z;

  friend std::ostream& operator<<(std::ostream& out, const vec3& self) {
    return out << self.x << " " << self.y << " " << self.z;
  }
};

template<>
struct traits<vec3> {
  using scalar = real;
  
  static const std::string name() { return "vec3"; }
  static vec3 parse(std::istream& in) {
    scalar x, y, z;
    
    in >> x >> y >> z;
    return {x, y, z};
  }
};


template<>
struct traits<std::size_t> {
  static const std::string name() { return "std::size_t"; }
  static std::size_t parse(std::istream& in) {
    std::size_t res;
    in >> res;
    return res;
  }
};


// a component is an object in the scene graph
struct component : object {
  component(const node& ) { }
};


// some component class
template<class T>
struct state : component {
  using component::component;

  T pos;
  std::size_t foo;

  static const info_type info;
};

template<class T>
const info_type state<T>::info = declare< state >("state")
                            (&state::pos, "pos", "position vector")
                            (&state::foo, "foo", "some data");  

template<class T>
struct derived_state : state<T> {
  using state<T>::state;
  
  std::size_t bar;

  static const info_type info;
};


template<class T>
const info_type derived_state<T>::info = declare< derived_state, state<T> >("derived_state")
  (&derived_state::bar, "bar", "some other data");

// TODO data dependency graph?

struct data_node {
  std::shared_ptr<object> obj;
  std::string name;
  
  template<class T>
  T* get() { return obj->data<T>(name); }
};


struct engine_base {
  virtual ~engine_base() { }

  virtual std::function< void() > update(data_node out, data_node* first, data_node* last) const = 0;
};


struct engine_node {
  std::shared_ptr<engine_base> engine;
  std::function< void() > push;
};


template<class T> struct engine;

template<class Ret, class ... Args>
struct engine< Ret(Args...) > : engine_base {
  virtual void apply(Ret& out, const Args&... args) const = 0;
};


template<class T>
struct sum : engine< T(T, T) > {
  
  void apply(T& out, const T& lhs, const T& rhs) const {
    out = lhs + rhs;
  }

  std::function< void() > update(data_node out, data_node* first, data_node* last) const {
    const T* lhs_buf = first[0].get<T>();
    const T* rhs_buf = first[1].get<T>();
    
    T* out_buf = out.get<T>();
    
    return [lhs_buf, rhs_buf, out_buf, this] {
      this->apply(*out_buf, *lhs_buf, *rhs_buf);
    };
    
  }
  
};


#include "graph.hpp"

struct vertex {
  void* key;
  enum kind_type { data, engine } kind;

  vertex(void* key, kind_type kind) : key(key), kind(kind) { }
  
  vertex* first = nullptr;
  vertex* next = nullptr;
  bool marked;
  
  friend std::ostream& operator<<(std::ostream& out, const vertex& self) {
    return out << (self.kind == data ? "data" : "engine") << ": " << self.key;
  }
};


namespace graph {
  template<class Key>
  struct traits< std::map<Key, vertex> > {
    using graph_type = std::map<Key, vertex>;
    using ref_type = vertex*;

    static ref_type& first(const graph_type& g, const ref_type& v) { return v->first; }
    static ref_type& next(const graph_type& g, const ref_type& v) { return v->next; }

    static bool marked(const graph_type& g, const ref_type& v) { return v->marked; }
    static void marked(const graph_type& g, const ref_type& v, bool marked) { v->marked = marked; }

    template<class F>
    static void iter(graph_type& g, const F& f) { for(auto& it : g) { f(&it.second); } }
  };
}


struct data_graph {
  using graph_type = std::map<void*, vertex>;
  graph_type graph;

  // insert engine in dependency graph, given input and output data
  void add(engine_base* engine, data_node out, data_node* first, data_node* last) {
    using namespace graph;
    
    auto insert = graph.emplace(engine, vertex(engine, vertex::engine));

    // TODO setup vertex
    
    if(!insert.second) {
      throw std::runtime_error("engine already added");
    }

    vertex* e = &insert.first->second;

    // input edges
    for(data_node* it = first; it != last; ++it) {
      void* ptr = it->get<void>();
      
      vertex* v = &graph.emplace(ptr, vertex(ptr, vertex::data)).first->second;
      
      graph::connect(graph, e, v);
    }

    {
      // output edge
      void* ptr = out.get<void>();
      vertex* u = &graph.emplace(ptr, vertex(ptr, vertex::data)).first->second;
      
      graph::connect(graph, u, e);
    }
  }
  
};




// instantiations
template struct state<vec3>;
template struct derived_state<vec3>;  

#include <iostream>

int main(int, char**) {

  attributes attrs = attributes
    ("template", "vec3")
    ("pos", vec3{1, 2, 3} )
    ("foo", "14");
  
  node root;
  auto obj = root.create("derived_state", attrs);

  if( auto ptr = std::dynamic_pointer_cast< state<vec3> >(obj) ) {
    std::clog << ptr->pos << std::endl;
  }

  auto* data = obj->data<vec3>("pos");
  std::clog << "pos: " << *data << std::endl;


  // engine test
  auto p1 = root.create("state", attributes("template", "vec3"));
  auto p2 = root.create("state", attributes("template", "vec3"));
  auto p3 = root.create("state", attributes("template", "vec3"));  
  
  engine_base* f = new sum<real>();

  data_graph deps;

  data_node out = {p1, "pos"};
  data_node in[2] = {{p2, "pos"}, {p3, "pos"}};

  deps.add(f, out, in, in + 2);

  graph::dfs(deps.graph, [&](vertex* v) {
      std::clog << *v << std::endl;
    });

  
  return 0;
}


