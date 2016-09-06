#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

add_executable(chatroom-server
	../common/cr_stdafx.cpp
	../common/cr_stdafx.h
	../common/cr_chat_package.h
	../common/cr_chat_package.cpp
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.cc
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.h	
	${PACKET_FILES_OUTPUT_DIR}/ChatMessage.pb.cc
	${PACKET_FILES_OUTPUT_DIR}/ChatMessage.pb.h	
	crs_main.cpp
)

set_property(TARGET chatroom-server PROPERTY FOLDER "test/chatroom")

if(MSVC)
target_link_libraries(chatroom-server
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	libprotobuf.lib
)

else()

target_link_libraries(chatroom-server
	cyclone
	protobuf
	${JEMALLOC_LIBRARIES}
	pthread	
)
endif()
