#include <memory>
#include <cstddef>
#include <iostream>

struct header {
  std::size_t magic = 1234567890;
  
  header() {
    std::clog << __func__ << " " << this << std::endl;
  }

  ~header() {
    std::clog << magic << std::endl;
    std::clog << __func__ << " " << this << std::endl;
  }
};


struct item {
  item() {
    std::clog << __func__ << " " << this << std::endl;
  }

  ~item() {
    std::clog << __func__ << " " << this << std::endl;
  }
};


template<class H, class T>
struct variable {


  struct base {
    H header;
    T body[0];
  };
  
  struct boxed {
    T value;
    
    static void* operator new[](std::size_t count) {
      void* ptr = std::malloc( sizeof(base) + count );
      
      // initialize control block
      base* b = new (ptr) base;
      
      std::clog << "new: " << b << " " << b->body << std::endl;
      
      return b->body;
    };
    
    static void operator delete[](void* ptr, std::size_t size) {
      std::clog << "delete: " << ptr << std::endl;
      base* block = reinterpret_cast<base*>((char*)ptr - offsetof(base, body));
      
      // finalize control block
      block->~base();
      
      std::free(block);
    }


    friend const H& head(const boxed* self) {
      const base* block = reinterpret_cast<const base*>((const char*)self - offsetof(base, body));
      return block->header;
    }

    friend H& head(boxed* self) {
      base* block = reinterpret_cast<base*>((char*)self - offsetof(base, body));
      return block->header;
    }

    
      
  };
  
};


int main(int, char**) {

  using type = variable<header, item>;

  type::boxed* test = new type::boxed[14];

  std::clog << head(test).magic << std::endl;
  
  delete [] test;
  
  return 0;
}
