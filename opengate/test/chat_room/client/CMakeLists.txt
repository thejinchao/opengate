#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

if(MSVC)

add_executable(chatroom-client WIN32
	../common/cr_stdafx.cpp
	../common/cr_stdafx.h
	../common/cr_chat_package.h
	../common/cr_chat_package.cpp
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.cc
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.h	
	${PACKET_FILES_OUTPUT_DIR}/ChatMessage.pb.cc
	${PACKET_FILES_OUTPUT_DIR}/ChatMessage.pb.h	
	crc_win_main.cpp
	crc_chat_dialog.h
	crc_chat_dialog.cpp
	chatroom-client.rc
	resource.h
	)

else()

add_executable(chatroom-client 
	../common/cr_stdafx.cpp
	../common/cr_stdafx.h
	../common/cr_chat_package.h
	../common/cr_chat_package.cpp
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.cc
	${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.h	
	crc_main.cpp)

endif()

set_property(TARGET chatroom-client PROPERTY FOLDER "test/chatroom")

if(MSVC)
target_link_libraries(chatroom-client
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	libprotobuf.lib
)

else()

target_link_libraries(chatroom-client
	cyclone
	protobuf
	${JEMALLOC_LIBRARIES}
	pthread
)
endif()
