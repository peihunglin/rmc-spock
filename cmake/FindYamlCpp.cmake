# Locate yaml-cpp
#
# This module defines
#  YamlCpp_FOUND, if false, do not try to link to yaml-cpp
#  YamlCpp_LIBNAME, name of yaml library
#  YamlCpp_LIBRARY, where to find yaml-cpp
#  YamlCpp_LIBRARY_RELEASE, where to find Release or RelWithDebInfo yaml-cpp
#  YamlCpp_LIBRARY_DEBUG, where to find Debug yaml-cpp
#  YamlCpp_INCLUDE_DIR, where to find yaml.h
#  YamlCpp_LIBRARY_DIR, the directories to find YamlCpp_LIBRARY
#
# By default, the dynamic libraries of yaml-cpp will be found. To find the static ones instead,
# you must set the YamlCpp_USE_STATIC_LIBS variable to TRUE before calling find_package(YamlCpp ...)

# attempt to find static library first if this is set
if(YamlCpp_USE_STATIC_LIBS)
    set(YamlCpp_STATIC libyaml-cpp.a)
    set(YamlCpp_STATIC_DEBUG libyaml-cpp-dbg.a)
endif()

if(${MSVC})    ### Set Yaml libary name for Windows
  set(YamlCpp_LIBNAME "libyaml-cppmd" CACHE STRING "Name of YAML library")
  set(YamlCpp_LIBNAME optimized ${YamlCpp_LIBNAME} debug ${YamlCpp_LIBNAME}d)
else()                      ### Set Yaml libary name for Unix, Linux, OS X, etc
  set(YamlCpp_LIBNAME "yaml-cpp" CACHE STRING "Name of YAML library")
endif()

# find the yaml-cpp include directory
find_path(YamlCpp_INCLUDE_DIR
  NAMES yaml-cpp/yaml.h
  PATH_SUFFIXES include
  PATHS
    ${YamlCpp_ROOT}/include
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp-0.5.1/include
    ~/Library/Frameworks/yaml-cpp/include/
    /Library/Frameworks/yaml-cpp/include/
    /usr/local/include/
    /usr/include/
    /sw/yaml-cpp/         # Fink
    /opt/local/yaml-cpp/  # DarwinPorts
    /opt/csw/yaml-cpp/    # Blastwave
    /opt/yaml-cpp/)

# find the release yaml-cpp library
find_library(YamlCpp_LIBRARY_RELEASE
  NAMES ${YamlCpp_STATIC} yaml-cpp libyaml-cppmd.lib libyaml-cppmt.lib
  PATH_SUFFIXES lib64 lib Release RelWithDebInfo
  PATHS
    ${YamlCpp_ROOT}
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp-0.5.1/
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp-0.5.1/build
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt)

# find the debug yaml-cpp library
find_library(YamlCpp_LIBRARY_DEBUG
  NAMES ${YamlCpp_STATIC_DEBUG} yaml-cpp-dbg libyaml-cppmdd.lib libyaml-cppmtd.lib
  PATH_SUFFIXES lib64 lib Debug
  PATHS
    ${YamlCpp_ROOT}
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp-0.5.1/
    ${PROJECT_SOURCE_DIR}/dependencies/yaml-cpp-0.5.1/build
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt)

# set library vars
set(YamlCpp_LIBRARY ${YamlCpp_LIBRARY_RELEASE})
if(CMAKE_BUILD_TYPE MATCHES Debug AND EXISTS ${YamlCpp_LIBRARY_DEBUG})
  set(YamlCpp_LIBRARY ${YamlCpp_LIBRARY_DEBUG})
endif()

get_filename_component(YamlCpp_LIBRARY_RELEASE_DIR ${YamlCpp_LIBRARY_RELEASE} PATH)
get_filename_component(YamlCpp_LIBRARY_DEBUG_DIR ${YamlCpp_LIBRARY_DEBUG} PATH)
set(YamlCpp_LIBRARY_DIR ${YamlCpp_LIBRARY_RELEASE_DIR} ${YamlCpp_LIBRARY_DEBUG_DIR})

# handle the QUIETLY and REQUIRED arguments and set YamlCpp_FOUND to TRUE if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(YamlCpp DEFAULT_MSG
  YamlCpp_INCLUDE_DIR
  YamlCpp_LIBRARY
  YamlCpp_LIBRARY_DIR)
mark_as_advanced(
  YamlCpp_INCLUDE_DIR
  YamlCpp_LIBRARY_DIR
  YamlCpp_LIBRARY
  YamlCpp_LIBRARY_RELEASE
  YamlCpp_LIBRARY_RELEASE_DIR
  YamlCpp_LIBRARY_DEBUG
  YamlCpp_LIBRARY_DEBUG_DIR)