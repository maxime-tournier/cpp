project(slip VERSION 0.1)

add_executable(slip main.cpp ast.cpp type.cpp eval.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

option(SLIP_DEBUG "" OFF)
if(SLIP_DEBUG)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DSLIP_DEBUG)
endif()

target_link_libraries(${PROJECT_NAME} readline)


