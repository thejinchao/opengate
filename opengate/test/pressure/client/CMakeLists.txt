#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

add_executable(press-client 
	../common/pr_stdafx.cpp
	../common/pr_stdafx.h
	../common/pr_package.h
	../common/pr_package.cpp
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.cc
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.h	
	prc_main.cpp)

set_property(TARGET press-client PROPERTY FOLDER "test/pressure")

if(MSVC)
target_link_libraries(press-client
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	libprotobuf.lib
)

else()

target_link_libraries(press-client
	cyclone
	protobuf
	${JEMALLOC_LIBRARIES}
	pthread	
)
endif()
