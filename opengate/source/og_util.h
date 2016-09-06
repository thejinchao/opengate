#pragma once

namespace google {
	namespace protobuf {
		class Message;
	}
}

//build message send to game server
size_t build_server_msg_from_protobuf(uint16_t id, uint32_t agent_id, ::google::protobuf::Message* message, char* buf, size_t buf_length);
