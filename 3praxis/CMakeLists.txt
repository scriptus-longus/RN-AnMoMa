cmake_minimum_required (VERSION 3.5)
project (RN-Praxis)
set (CMAKE_C_STANDARD 11)

set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IRRNORE_FILES
  ${CMAKE_BINARY_DIR}
  /\\..*$
)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)

find_package(PkgConfig)
pkg_check_modules(PC_ZeroMQ QUIET zmq)
find_path(ZeroMQ_INCLUDE_DIR NAMES zmq.hpp PATHS ${PC_ZeroMQ_INCLUDE_DIRS})
find_library(ZeroMQ_LIBRARY NAMES zmq PATHS ${PC_ZeroMQ_LIBRARY_DIRS})


add_executable(zmq_distributor zmq_distributor.c)
add_executable(zmq_worker zmq_worker.c)

target_include_directories(zmq_worker PUBLIC ${ZeroMQ_INCLUDE_DIR})
target_link_libraries(zmq_worker PUBLIC ${ZeroMQ_LIBRARY})


