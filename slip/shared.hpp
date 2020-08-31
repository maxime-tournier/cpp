#ifndef SHARED_HPP
#define SHARED_HPP

#include <memory>

template<class Info>
struct shared: std::shared_ptr<const Info> {
  template<class... Args>
  explicit shared(const Args&... args):
    std::shared_ptr<const Info>(
          std::make_shared<const Info>(Info{args...})) {}

  shared(const shared&) = default;
  shared(shared&&) = default;  
  
};



#endif
