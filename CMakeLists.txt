cmake_minimum_required(VERSION 2.8)
project(Spock)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE Release)
endif()

# Extra optimization switches for GCC compilers
if(CMAKE_COMPILER_IS_GNUCXX AND (${CMAKE_BUILD_TYPE} STREQUAL "Release"))
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fomit-frame-pointer -DNDEBUG")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")  ## Strip binary
endif()

# Extra warning switches for GCC compilers
if(CMAKE_COMPILER_IS_GNUCXX)
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-local-typedefs")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-local-typedefs")
endif()

# What is the system thread library?
option(MULTI_THREADED "Should library be multi-thread aware?" YES)
if(NOT "${MULTI_THREADED}")
  message(STATUS "Multi-thread support is disabled")
  find_package(Boost 1.45.0 REQUIRED COMPONENTS iostreams system filesystem regex serialization)
else()
  message(STATUS "Multi-thread support is enabled")
  find_package(Threads)
  if( ${CMAKE_CXX_COMPILER_ID} MATCHES GNU )
    message(STATUS "  adding -pthread to compile commands")
    add_definitions(-pthread)
  elseif( ${CMAKE_CXX_COMPILER_ID} MATCHES Clang )
    message(STATUS "  adding -pthread to compile commands")
    add_definitions(-pthread)
  else()
    message(STATUS "not adding -pthread")
  endif()
  find_package(Boost 1.45.0 REQUIRED COMPONENTS date_time iostreams system filesystem regex thread serialization)
endif()

find_package(Sawyer 0.9.0 CONFIG REQUIRED)
find_package(YamlCpp MODULE REQUIRED)

set(lib_src
  src/Spock/Context.C
  src/Spock/GlobalFlag.C
  src/Spock/InstalledPackage.C
  src/Spock/Spock.C
  src/Spock/VersionNumber.C
)

add_library(spock SHARED ${lib_src})
target_link_libraries(spock ${Sawyer_LIBRARY} ${YamlCpp_LIBRARY} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${YamlCpp_INCLUDE_DIR}
  ${Sawyer_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
)

add_executable(spock-employ src/spock-employ.C)
target_link_libraries(spock-employ spock)
