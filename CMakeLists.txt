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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-local-typedefs -Wno-missing-field-initializers")
endif()

# What is the system thread library?
option(MULTI_THREADED "Should library be multi-thread aware?" YES)
if(NOT "${MULTI_THREADED}")
  message(STATUS "Multi-thread support is disabled")
  find_package(Boost 1.45.0 REQUIRED COMPONENTS iostreams system filesystem random regex serialization)
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
  find_package(Boost 1.45.0 REQUIRED COMPONENTS date_time iostreams system filesystem random regex thread serialization)
endif()

find_package(Sawyer 0.9.0 CONFIG REQUIRED)
find_package(YamlCpp MODULE REQUIRED)

set(lib_src
  src/Spock/Context.C
  src/Spock/DefinedPackage.C
  src/Spock/Directory.C
  src/Spock/Environment.C
  src/Spock/GhostPackage.C
  src/Spock/GlobalFlag.C
  src/Spock/InstalledPackage.C
  src/Spock/Package.C
  src/Spock/PackagePattern.C
  src/Spock/PackageLists.C
  src/Spock/Solver.C
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

# The executables in the build tree should have an RPATH that points
# to the build tree, and when we install those exectables we need to
# relink them to point to the library installation directory.
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) # Don't use install path in builddir
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE) # auto add stuff to install path

# the RPATH to be used when installing, but only if it's not a system directory
list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
if("${isSystemDir}" STREQUAL "-1")
   set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()

add_executable(spock-shell src/spock-shell.C)
target_link_libraries(spock-shell spock)

add_executable(spock-ls src/spock-ls.C)
target_link_libraries(spock-ls spock)

add_executable(spock-compiler src/spock-compiler.C)
target_link_libraries(spock-compiler spock)

add_executable(spock-using src/spock-using.C)
target_link_libraries(spock-using spock)

add_executable(spock-rm src/spock-rm.C)
target_link_libraries(spock-rm spock)

add_executable(spock-download src/spock-download.C)
target_link_libraries(spock-download spock)

#################### Installation ####################

install(
  TARGETS
    spock spock-shell spock-ls spock-compiler spock-using spock-rm spock-download
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)

install(
  PROGRAMS
    scripts/rmc
  DESTINATION bin
)

install(
  PROGRAMS
    scripts/spock-install-system-compilers
    scripts/spock-compiler-install-program
    scripts/spock-os-name
    scripts/rose-install-all-deps
    scripts/rose-install-intel-compilers
  DESTINATION scripts
)

install(
  PROGRAMS  
    scripts/impl/detect-compiler-characteristics
    scripts/impl/install-compiler-executable
  DESTINATION scripts/impl
)

install(
  FILES
    scripts/impl/basic-support.sh
    scripts/impl/compiler-support.sh
    scripts/impl/installation-support.sh
    scripts/impl/spock-realpath.sh
  DESTINATION scripts/impl
)

install(
  PROGRAMS
    scripts/rmc-commands/config
    scripts/rmc-commands/make
    scripts/rmc-commands/init
  DESTINATION scripts/rmc-commands
)

file(GLOB package_files "${CMAKE_CURRENT_SOURCE_DIR}/lib/packages/*.yaml")

install(
  FILES ${package_files}
  DESTINATION lib/packages
)
