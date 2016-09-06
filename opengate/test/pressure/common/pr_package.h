#pragma once

namespace google {
	namespace protobuf {
		class Message;
	}
}

class Package
{
public:
	void buildFromMessage(const char* message, size_t msgLen);
	void buildFromProtobuf(uint16_t id, ::google::protobuf::Message* message);
	bool buildFromBuf(RingBuf& buf);

	const char* get_buf(void) const { return m_buf; }
	size_t get_buf_length(void) const { return m_buf_length; }

	uint16_t get_packetsize(void) const { return socket_api::ntoh_16(*m_packetSize); }
	uint16_t get_Packet_id(void) const { return socket_api::ntoh_16(*m_packetID); }
	uint32_t get_Packet_timestamp(void) const { return socket_api::ntoh_32(*m_packetTimeStamp); }

	const char* get_content(void) const { return m_content; }

protected:
	enum { HEAD_SIZE = 8 };

	uint16_t* m_packetSize;
	uint16_t* m_packetID;
	uint32_t* m_packetTimeStamp;
	char*	 m_content;

	char* m_buf;
	size_t m_buf_length;

public:
	Package();
	~Package();
};
