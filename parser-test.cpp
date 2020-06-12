#include "parser.hpp"

#include <gtest/gtest.h>

using namespace parser;

template<class Parser>
static void assert_parse(Parser parser, std::string input) {
  range in{input.data(), input.data() + input.size()};
  ASSERT_TRUE(parser(in)) << input;
}


TEST(parser, keyword) {
  assert_parse(keyword("foo"), "foo");
  assert_parse(keyword("foo"), "foo ");  
}
