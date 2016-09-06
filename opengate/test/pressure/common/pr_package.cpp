#include "pr_stdafx.h"
#include "pr_package.h"

#include "GateMessage.pb.h"

//-------------------------------------------------------------------------------------
Package::Package()
	: m_packetSize(0)
	, m_packetID(0)
	, m_packetTimeStamp(0)
	, m_content(0)
	, m_buf(0)
{

}

//-------------------------------------------------------------------------------------
Package::~Package()
{
	if (m_buf)
	{
		delete[] m_buf;
	}
}

//-------------------------------------------------------------------------------------
void Package::buildFromMessage(const char* message, size_t msgLen)
{
	m_buf_length = msgLen + HEAD_SIZE;
	m_buf = new char[m_buf_length];

	m_packetSize = (uint16_t*)(m_buf);
	m_packetID = (uint16_t*)(m_buf + 2);
	m_packetTimeStamp = (uint32_t*)(m_buf + 4);
	m_content = m_buf + HEAD_SIZE;

	*m_packetSize = (uint16_t)socket_api::ntoh_16((uint16_t)(msgLen & 0xFFFF));
	*m_packetID = (uint16_t)socket_api::ntoh_16((uint16_t)(10001));
	*m_packetTimeStamp = 0;
	memcpy(m_content, message, msgLen);
}

//-------------------------------------------------------------------------------------
void Package::buildFromProtobuf(uint16_t id, ::google::protobuf::Message* message)
{
	int msgLen = message->ByteSize();

	m_buf_length = msgLen + HEAD_SIZE;
	m_buf = new char[m_buf_length];

	m_packetSize = (uint16_t*)(m_buf);
	m_packetID = (uint16_t*)(m_buf + 2);
	m_packetTimeStamp = (uint32_t*)(m_buf + 4);
	m_content = m_buf + HEAD_SIZE;

	*m_packetSize = (uint16_t)socket_api::ntoh_16((uint16_t)(msgLen & 0xFFFF));
	*m_packetID = (uint16_t)socket_api::ntoh_16((uint16_t)(id));
	*m_packetTimeStamp = 0;

	message->SerializeToArray(m_content, msgLen);
}

//-------------------------------------------------------------------------------------
bool Package::buildFromBuf(RingBuf& buf)
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
	m_packetTimeStamp = (uint32_t*)(m_buf + 4);
	m_content = m_buf + HEAD_SIZE;

	return true;
}
