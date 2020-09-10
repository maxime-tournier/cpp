project(cpp CXX)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)
option(ASAN "" OFF)

if(ASAN)
  set(ASAN_FLAGS "-fsanitize=address -fsanitize-address-use-after-scope")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}" )
endif()

# add_subdirectory(sparse)
if(ANDROID)
  set(CMAKE_EXE_LINKER_FLAGS -latomic)
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -march=native -fexceptions)
  set(LUA_MATH_LIBRARY "/system/lib/libm.so")
else()
endif()
  
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(compressed compressed.cpp)
add_executable(hamt hamt.cpp)

add_subdirectory(slip)
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
option(BUILD_TESTS "" OFF)
if(BUILD_TESTS)
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
endif()
