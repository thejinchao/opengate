#include "cr_stdafx.h"
#include "cr_chat_package.h"

#include "GateMessage.pb.h"

//-------------------------------------------------------------------------------------
ChatPackage::ChatPackage()
	: m_packetSize(0)
	, m_packetID(0)
	, m_packetAgentID(0)
	, m_content(0)
	, m_buf(0)
{

}

//-------------------------------------------------------------------------------------
ChatPackage::~ChatPackage()
{
	if (m_buf)
	{
		delete[] m_buf;
	}
}

//-------------------------------------------------------------------------------------
void ChatPackage::buildFromProtobuf(uint16_t id, uint32_t agent_id, ::google::protobuf::Message* message, XorShift128* seed)
{
	int msgLen = message->ByteSize();

	m_buf_length = msgLen + HEAD_SIZE;
	m_buf = new char[m_buf_length];

	m_packetSize = (uint16_t*)(m_buf);
	m_packetID = (uint16_t*)(m_buf + 2);
	m_packetAgentID = (uint32_t*)(m_buf + 4);
	m_content = m_buf + HEAD_SIZE;

	*m_packetSize = (uint16_t)socket_api::ntoh_16((uint16_t)(msgLen & 0xFFFF));
	*m_packetID = (uint16_t)socket_api::ntoh_16((uint16_t)(id));
	*m_packetAgentID = (uint32_t)socket_api::ntoh_32((uint32_t)(agent_id));

	message->SerializeToArray(m_content, msgLen);

	if (seed != 0) {
		xorshift128((uint8_t*)m_content, msgLen, *seed);
	}
}

//-------------------------------------------------------------------------------------
bool ChatPackage::buildFromRingBuf(RingBuf& buf, XorShift128* seed)
{
	size_t buf_len = buf.size();

	uint16_t packetSize;
	if (sizeof(packetSize) != buf.peek(0, &packetSize, sizeof(packetSize))) return false;
	packetSize = socket_api::ntoh_16(packetSize);

	if (buf_len < HEAD_SIZE + (size_t)packetSize) return false;


	m_buf_length = packetSize + HEAD_SIZE;
	m_buf = new char[m_buf_length];

	buf.memcpy_out(m_buf, m_buf_length);

	m_packetSize = (uint16_t*)(m_buf);
	m_packetID = (uint16_t*)(m_buf + 2);
	m_packetAgentID = (uint32_t*)(m_buf + 4);
	m_content = m_buf + HEAD_SIZE;

	if (seed != 0) {
		xorshift128((uint8_t*)m_content, packetSize, *seed);
	}

	return true;
}
