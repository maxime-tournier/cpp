project(cpp)
cmake_minimum_required(VERSION 3.10)

set(ASAN_FLAGS "-fsanitize=address -fsanitize-address-use-after-scope")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_FLAGS}" )

add_subdirectory(sparse)

set(CMAKE_CXX_STANDARD 14)

add_executable(compressed compressed.cpp)

add_executable(parser parser.cpp)
target_link_libraries(parser m)

add_executable(obj obj.cpp)

# mesh viewer
find_package(Qt5 COMPONENTS Widgets)
find_package(Eigen3)
find_package(OpenGL)
add_executable(mesh mesh.cpp)
target_link_libraries(mesh Qt5::Widgets Eigen3::Eigen OpenGL::GL)
target_compile_definitions(mesh PUBLIC QT_NO_KEYWORDS)


# tests
find_package(GTest)

if(GTest_FOUND) 
  enable_testing()
  
  add_executable(tests
				 parser-test.cpp)
  
  target_link_libraries(tests GTest::GTest GTest::Main)
  gtest_discover_tests(tests)
endif()
  
