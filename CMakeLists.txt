project(dellabaduck)
cmake_minimum_required(VERSION 3.2)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(
  dellabaduck
  dellabaduck.cpp
  str.cpp
  time.cpp
)

# disable assert()
#add_definitions(-DNDEBUG)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
target_link_libraries(dellabaduck Threads::Threads)
