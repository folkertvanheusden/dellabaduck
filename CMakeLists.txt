project(dellabaduck)
cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add_compile_options(-Wall -pedantic -DSTORE_1_PLAYOUT)
add_compile_options(-Wall -pedantic)

add_executable(
  board_unittest
  board.cpp
  board_unittest.cpp
  random.cpp
  vertex.cpp
  zobrist.cpp
)

add_executable(
  dellabaduck
  board.cpp
  dellabaduck.cpp
  dump.cpp
  helpers.cpp
  io.cpp
  random.cpp
  score.cpp
  str.cpp
  time.cpp
  uct.cpp
  unittest.cpp
  vertex.cpp
  zobrist.cpp
)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(dellabaduck Threads::Threads)
