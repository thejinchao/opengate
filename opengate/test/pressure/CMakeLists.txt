#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/common
	$ENV{CYCLONE_SDK_ROOT}/include
	$ENV{PROTOBUF_SDK_ROOT}/include
	${PACKET_FILES_OUTPUT_DIR}
)

link_directories(
	$ENV{CYCLONE_SDK_ROOT}/lib
	$ENV{PROTOBUF_SDK_ROOT}/lib
)

add_subdirectory(client)
add_subdirectory(server)
