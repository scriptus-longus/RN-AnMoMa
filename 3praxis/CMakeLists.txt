cmake_minimum_required (VERSION 3.5)
project (RN-Praxis)
set (CMAKE_C_STANDARD 11)

find_package(Threads REQUIRED)

find_package(PkgConfig)
pkg_check_modules(PC_ZeroMQ QUIET zmq)
find_path(ZeroMQ_INCLUDE_DIR NAMES zmq.hpp PATHS ${PC_ZeroMQ_INCLUDE_DIRS})
find_library(ZeroMQ_LIBRARY NAMES zmq PATHS ${PC_ZeroMQ_LIBRARY_DIRS})


add_executable(zmq_distributor zmq_distributor.c util.h counter.h)
add_executable(zmq_worker zmq_worker.c util.h counter.h)

target_include_directories(zmq_worker PUBLIC ${ZeroMQ_INCLUDE_DIR})
target_link_libraries(zmq_worker PUBLIC ${ZeroMQ_LIBRARY})

target_link_libraries(zmq_worker PUBLIC Threads::Threads)

target_include_directories(zmq_distributor PUBLIC ${ZeroMQ_INCLUDE_DIR})
target_link_libraries(zmq_distributor PUBLIC ${ZeroMQ_LIBRARY})

set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  ${CMAKE_BINARY_DIR}
  /\\..*$
)
set(CPACK_VERBATIM_VARIABLES YES)
set(THREADS_PREFER_PTHREAD_FLAG ON)
include(CPack)
