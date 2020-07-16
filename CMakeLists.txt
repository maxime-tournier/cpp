project(cpp CXX)
cmake_minimum_required(VERSION 3.10)

# set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)
set(ASAN_FLAGS "-fsanitize=address -fsanitize-address-use-after-scope")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_FLAGS}" )

# add_subdirectory(sparse)
if(ANDROID)
  message("android")
  set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES -latomic)
else()
  message("not android")
endif()
  
set(CMAKE_CXX_STANDARD 14)

add_executable(compressed compressed.cpp)
add_executable(hamt hamt.cpp)

add_executable(parser parser.cpp)
target_link_libraries(parser m)

add_executable(slip slip.cpp ast.cpp type.cpp)
target_link_libraries(slip readline)


add_executable(obj obj.cpp)

# add_executable(cml cml.cpp)

# mesh viewer
option(BUILD_MESH "" OFF)
if(BUILD_MESH)
  find_package(Qt5 COMPONENTS Widgets)
  find_package(Eigen3)
  cmake_policy(SET CMP0072 NEW)
  find_package(OpenGL)
  find_package(GLEW)
  add_executable(mesh mesh.cpp)
  target_link_libraries(mesh Qt5::Widgets Eigen3::Eigen OpenGL::GL GLEW::glew)
  target_compile_definitions(mesh PUBLIC QT_NO_KEYWORDS)
endif()

# tests
find_package(GTest)

if(GTest_FOUND) 
  enable_testing()
  
  add_executable(tests
				 parser-test.cpp
				 hamt-test.cpp
				 variant.cpp)
  
  target_link_libraries(tests GTest::GTest GTest::Main)
  gtest_discover_tests(tests)
endif()
  
