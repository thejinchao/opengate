#include "og_stdafx.h"
#include "og_util.h"

#include "GateMessage.pb.h"
#include "og_message_define.h"

//-------------------------------------------------------------------------------------
size_t build_server_msg_from_protobuf(uint16_t id, uint32_t agent_id, ::google::protobuf::Message* message, char* buf, size_t buf_length)
{
	int32_t msgLen = message->ByteSize();

	size_t needLength = msgLen + GAME_MESSAGE_HEAD_SIZE;
	if (needLength > buf_length) return needLength;

	uint16_t* packetSize = (uint16_t*)(buf);
	uint16_t* packetID = (uint16_t*)(buf + 2);
	uint32_t* agentId = (uint32_t*)(buf + 4);
	char* content = buf + GAME_MESSAGE_HEAD_SIZE;

	*packetSize = (uint16_t)socket_api::ntoh_16((uint16_t)(msgLen & 0xFFFF));
	*packetID = (uint16_t)socket_api::ntoh_16((uint16_t)(id));
	*agentId = (uint32_t)socket_api::ntoh_32((uint32_t)(agent_id));

	message->SerializeToArray(content, msgLen);
	return needLength;
}

