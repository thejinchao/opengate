#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

project(OpenGate)

########
#build info
########
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/../bin)
string(TIMESTAMP OPENGATE_BUILD_TIME "%Y-%m-%d-%H-%M-%S")

########
#find jemalloc library
########
find_path(JEMALLOC_SDK_ROOT
    NAMES include/jemalloc/jemalloc.h
    HINTS $ENV{JEMALLOC_SDK_ROOT}
)
find_library(JEMALLOC_LIBRARIES
    NAMES jemalloc_pic
    HINTS ${JEMALLOC_SDK_ROOT}/lib
)
find_path(JEMALLOC_INCLUDE_DIR
    NAMES jemalloc/jemalloc.h
    HINTS ${JEMALLOC_SDK_ROOT}/include
)

if(JEMALLOC_SDK_ROOT AND JEMALLOC_LIBRARIES)
  set(CY_HAVE_JEMALLOC_LIBRARIES TRUE)
  message(${JEMALLOC_LIBRARIES})
else()
  set(CY_HAVE_JEMALLOC_LIBRARIES FALSE)
  set(JEMALLOC_LIBRARIES "")
endif()

mark_as_advanced(
	JEMALLOC_LIBRARIES
	JEMALLOC_INCLUDE_DIR
)

if(CY_HAVE_JEMALLOC_LIBRARIES)
include_directories(
	${JEMALLOC_INCLUDE_DIR}
)
endif()

########
#compiler flag
########
if(MSVC)
	option(CY_USE_MULTITHREAD "Use Multi-thread /MT" OFF)
endif()


if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
	if(CY_USE_MULTITHREAD)
		set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} /MTd /W4 /ZI")
		set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} /MT /W4 /Zi")
	else()
		set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} /MDd /W4 /ZI")
		set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} /MD /W4 /Zi")
	endif()
else()
	set(CMAKE_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -g -O2 -std=c++0x -rdynamic -Wall -Wextra -Werror -Wconversion -Wno-unused-parameter	-Woverloaded-virtual -Wpointer-arith -Wshadow -Wwrite-strings -Wno-deprecated")
endif()

add_definitions(-DBUILD_TIME="${OPENGATE_BUILD_TIME}")

set(PACKET_FILES_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/_proto")
file(MAKE_DIRECTORY ${PACKET_FILES_OUTPUT_DIR})

########
#sub dictionary
########
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_subdirectory(source)
add_subdirectory(test)
