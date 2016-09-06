#pragma once

namespace google {
	namespace protobuf {
		class Message;
	}
}

class ChatPackage
{
public:
	void buildFromProtobuf(uint16_t id, uint32_t agent_id, ::google::protobuf::Message* message, XorShift128* seed);
	bool buildFromRingBuf(RingBuf& buf, XorShift128* seed);

	const char* get_buf(void) const { return m_buf; }
	size_t get_buf_length(void) const { return m_buf_length; }

	uint16_t get_packet_size(void) const { return socket_api::ntoh_16(*m_packetSize); }
	uint16_t get_packet_id(void) const { return socket_api::ntoh_16(*m_packetID); }

	uint32_t get_agent_id(void) const { return socket_api::ntoh_32(*m_packetAgentID); }
	void set_agent_id(uint32_t agent_id) { *m_packetAgentID = socket_api::ntoh_32(agent_id); }

	const char* get_content(void) const { return m_content; }

protected:
	enum { HEAD_SIZE = 8 };

	uint16_t* m_packetSize;
	uint16_t* m_packetID;
	uint32_t* m_packetAgentID;
	char*	 m_content;

	char* m_buf;
	size_t m_buf_length;


public:
	ChatPackage();
	~ChatPackage();
};
