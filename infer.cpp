#include "infer.hpp"

////////////////////////////////////////////////////////////////////////////////
namespace infer {

const kind term = std::make_shared<const kind_constant>("*");

const mono func =
    std::make_shared<const type_constant>("->", term >>= term >>= term);

}

////////////////////////////////////////////////////////////////////////////////
int main(int, char**) {

  return 0;
};
