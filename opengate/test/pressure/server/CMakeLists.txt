#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

add_executable(press-server
	../common/pr_stdafx.cpp
	../common/pr_stdafx.h
	../common/pr_package.h
	../common/pr_package.cpp
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.cc
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.h	
	prs_main.cpp
)

set_property(TARGET press-server PROPERTY FOLDER "test/pressure")

if(MSVC)
target_link_libraries(press-server
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	libprotobuf.lib
)

else()

target_link_libraries(press-server
	cyclone
	protobuf
	${JEMALLOC_LIBRARIES}
	pthread	
)
endif()
