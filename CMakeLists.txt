set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize-address-use-after-scope")

add_subdirectory(sparse)

set(CMAKE_CXX_STANDARD 14)

add_executable(compressed compressed.cpp)

add_executable(parser parser.cpp)
target_link_libraries(parser m)

add_executable(obj obj.cpp)


option(BUILD_TESTS "" ON)

if(BUILD_TESTS) 
  enable_testing()

  find_package(GTest REQUIRED)
  
  add_executable(tests
				 parser-test.cpp)
  
  target_link_libraries(tests GTest::GTest GTest::Main)
  gtest_discover_tests(tests)
endif()
  
