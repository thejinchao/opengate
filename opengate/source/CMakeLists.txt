#
#Copyright(C) thecodeway.com
#

cmake_minimum_required (VERSION 2.6)

########
#generate protobuf files
########

set(PROTO_FILES_PATH "$ENV{PROTO_FILES_PATH}" CACHE STRING "Protobuf Files Path")
execute_process(COMMAND "python"
		ID_Generator.py
	  WORKING_DIRECTORY ${PROTO_FILES_PATH}	)
execute_process(COMMAND "python"
		Msg_Generator.py
	  WORKING_DIRECTORY ${PROTO_FILES_PATH}	)
	  
file(COPY "${PROTO_FILES_PATH}/_temp/"
		DESTINATION ${PACKET_FILES_OUTPUT_DIR} 
		FILES_MATCHING PATTERN "*.h"
		)
file(COPY "${PROTO_FILES_PATH}/_temp/"
		DESTINATION ${PACKET_FILES_OUTPUT_DIR} 
		FILES_MATCHING PATTERN "*.cc"
		)
file(REMOVE_RECURSE "${PROTO_FILES_PATH}/_temp/")

if(UNIX)
	set(IGNORE_WARNING_CODE "\#ifdef _MSC_VER\\n\#pragma warning \(disable: 4512 4244 4127 4100\)\\n\#else\\n\#pragma GCC diagnostic ignored \\\"-Wshadow\\\"\\n\#pragma GCC diagnostic ignored \\\"-Wconversion\\\"\\n\#endif\\n")
	
	execute_process(COMMAND sed
		"6a ${IGNORE_WARNING_CODE}" GateMessage.pb.h
		WORKING_DIRECTORY ${PACKET_FILES_OUTPUT_DIR}
		OUTPUT_FILE  GateMessage.pb.h._temp)
		
	execute_process(COMMAND mv
		GateMessage.pb.h._temp GateMessage.pb.h
		WORKING_DIRECTORY ${PACKET_FILES_OUTPUT_DIR})
		
endif()

set(PROTO_PACKET_SOURCE_FILES 
	"${PACKET_FILES_OUTPUT_DIR}/HOpCode.cc"
	"${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.cc"
)
set(PROTO_PACKET_HEAD_FILES 
	"${PACKET_FILES_OUTPUT_DIR}/HOpCode.h"
	"${PACKET_FILES_OUTPUT_DIR}/GateMessage.pb.h"
)

########
#project setting
########
include_directories(
	$ENV{CYCLONE_SDK_ROOT}/include
	$ENV{PROTOBUF_SDK_ROOT}/include
	${PACKET_FILES_OUTPUT_DIR}
)

if(UNIX)
include_directories(
	$ENV{HIREDIS_SDK_ROOT}/include
)

set(HIREDIS_LIB_FILE 
	$ENV{HIREDIS_SDK_ROOT}/lib/libhiredis.a
)

endif()

link_directories(
	$ENV{CYCLONE_SDK_ROOT}/lib
	$ENV{PROTOBUF_SDK_ROOT}/lib
)

########
#Down Service
########
set(OPENGATE_DOWN_SERVICE_INC
	og_down_service.h
)
source_group("Down Service" FILES ${OPENGATE_DOWN_SERVICE_INC})

set(OPENGATE_DOWN_SERVICE_CPP
	og_down_service.cpp
	og_down_service_message.cpp
)
source_group("Down Service" FILES ${OPENGATE_DOWN_SERVICE_CPP})

########
#AgentService
########
set(OPENGATE_AGENT_SERVICE_INC
	og_agent_service.h
	og_agent_thread.h
	og_agent.h
)
source_group("Agent Service" FILES ${OPENGATE_AGENT_SERVICE_INC})

set(OPENGATE_AGENT_SERVICE_CPP
	og_agent_service.cpp
	og_agent_thread.cpp
	og_agent.cpp
)
source_group("Agent Service" FILES ${OPENGATE_AGENT_SERVICE_CPP})

########
#UpService
########
set(OPENGATE_UP_SERVICE_INC
	og_up_service.h
	og_up_thread.h
)
source_group("Up Service" FILES ${OPENGATE_UP_SERVICE_INC})

set(OPENGATE_UP_SERVICE_CPP
	og_up_service.cpp
	og_up_thread.cpp
)
source_group("Up Service" FILES ${OPENGATE_UP_SERVICE_CPP})

########
#ProtobufFiles
########
source_group("Protobuf Files" FILES 
	${PROTO_PACKET_HEAD_FILES}
	${PROTO_PACKET_SOURCE_FILES}
)

add_executable(opengate
	og_stdafx.h
	og_stdafx.cpp
	og_main.cpp
	og_config.cpp
	og_config.h
	og_message_define.h
	og_signal_handler.cpp
	og_signal_handler.h
	og_util.h
	og_util.cpp
	og_debug.h
	og_debug.cpp
	${OPENGATE_DOWN_SERVICE_INC}
	${OPENGATE_DOWN_SERVICE_CPP}
	${OPENGATE_AGENT_SERVICE_INC}
	${OPENGATE_AGENT_SERVICE_CPP}
	${OPENGATE_UP_SERVICE_INC}
	${OPENGATE_UP_SERVICE_CPP}
	${PROTO_PACKET_HEAD_FILES}
	${PROTO_PACKET_SOURCE_FILES}
)

if (MSVC)
	target_compile_options(opengate PRIVATE "/Fd${PROJECT_BINARY_DIR}/source/$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>/opengate.pdb") 
	add_definitions(-D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)

	set_target_properties(opengate PROPERTIES LINK_FLAGS_RELEASE "/DEBUG")

endif()
	

if(MSVC)
target_link_libraries(opengate
	cyclone
	ws2_32.lib
	shlwapi.lib
	winmm.lib
	libprotobuf.lib
	${JEMALLOC_LIBRARIES}
)

else()

target_link_libraries(opengate
	cyclone
	protobuf
	${HIREDIS_LIB_FILE}
	${JEMALLOC_LIBRARIES}
	pthread
)
endif()
	
	
########
#INSTALL
########
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/opengate.ini
	DESTINATION . )

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/startup.sh
	DESTINATION . )

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/shutdown.sh
	DESTINATION . )
	
install(TARGETS opengate
	DESTINATION .)
