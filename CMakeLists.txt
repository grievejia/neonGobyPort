cmake_minimum_required(VERSION 3.0)
project(neongoby)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Check for C++14 support
include (CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if (NOT COMPILER_SUPPORTS_CXX14)
	message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
endif()

# Set compilation flag
set (CMAKE_CXX_FLAGS " -std=c++14 -fno-rtti -Wall -fPIC -fdiagnostics-color=always")
set (CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set (CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Specify include, library and exec dir
set (PROJECT_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/include)
set (PROJECT_LIBRARY_PATH ${PROJECT_SOURCE_DIR}/lib)
set (PROJECT_TOOL_PATH ${PROJECT_SOURCE_DIR}/tool)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
include_directories(${PROJECT_INCLUDE_PATH})

# Specify library and binary output dir
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory (lib)
add_subdirectory (runtime)
add_subdirectory (tools)
