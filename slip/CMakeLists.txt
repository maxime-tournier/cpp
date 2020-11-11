project(slip VERSION 0.1)

add_executable(slip main.cpp sexpr.cpp ast.cpp type.cpp lua.cpp repl.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

option(SLIP_DEBUG "" OFF)
if(SLIP_DEBUG)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DSLIP_DEBUG)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE -DSLIP_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${PROJECT_NAME} readline)

find_package(Lua 5.3 REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${LUA_LIBRARIES})


