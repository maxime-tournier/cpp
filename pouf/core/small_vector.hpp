#ifndef SMALL_VECTOR_HPP
#define SMALL_VECTOR_HPP

#include "slice.hpp"

// a vector that is statically allocated for small sizes (1) and dynamically
// allocated for larger
template<class T>
class small_vector : public slice<T> {
  typename std::aligned_union<0, T>::type storage;
protected:

  T* fixed() {
    return reinterpret_cast<T*>(&storage);
  }

  const T* fixed() const {
    return storage.data();
  }
  
  
  void release() {
    switch(this->size()) {
    case 0: break;
    case 1:
      fixed()->~T();
      break;
    default:
      delete [] this->first;
    }

    this->last = this->first;
  }

  void alloc(std::size_t n) {
    switch(n) {
    case 0: break;
    case 1:
      this->first = new (fixed()) T;
      this->last = this->first + 1;
      break;
    default:
      this->first = new T[n];
      this->last = this->first + n;
    }
  }
  
public:
  small_vector(std::size_t n = 1) {
    alloc(n);
  }
  
  ~small_vector() {
    release();
  }
  
  small_vector(const small_vector& ) = delete;
  small_vector& operator=(const small_vector& ) = delete;  
  
  void resize(std::size_t n) {
    release();
    alloc(n);
  }


};



#endif
