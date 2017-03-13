#include "module.hpp"
#include <vector>

namespace python {

  // nifty
  static std::vector< void(*)() >* callbacks;
  
  registration::registration( void (*cb)() ) {
	static std::vector< void(*)() > storage;
	callbacks = &storage;
	
	callbacks->push_back(cb);
  }

  void init_module() {
	for(auto cb : *callbacks) {
	  cb();
	}
  }

}
