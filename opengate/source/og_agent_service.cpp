#include "og_stdafx.h"
#include "og_agent_service.h"
#include "og_agent_thread.h"
#include "og_debug.h"
#include "og_message_define.h"
#include "og_config.h"
#include "HOpCode.h"

#include <sys/stat.h>
#include <sys/types.h>

//-------------------------------------------------------------------------------------
AgentService* AgentService::s_singleton = 0;

//-------------------------------------------------------------------------------------
AgentService::AgentService()
	: m_thread_counts(0)
	, m_config(0)
{
	s_singleton = this;
	memset(m_thread_pool, 0, sizeof(AgentThread*)*MAX_THREAD_COUNTS);
}

//-------------------------------------------------------------------------------------
AgentService::~AgentService()
{
	s_singleton = 0;

	for (int32_t i = 0; i < m_thread_counts; i++)
	{
		delete m_thread_pool[i];
	}
}

//-------------------------------------------------------------------------------------
bool AgentService::start(int32_t thread_counts, Config* config)
{
	assert(m_thread_counts == 0);
	assert(thread_counts > 0);
	assert(config);

	m_next_thread_id = 0;

	//begin from 1
	m_next_agent_id = 1;

	m_config = config;

	//load packet desc
	const protobuf::message::MessageDesc* desc = protobuf::message::g_message_desc;
	while (desc->id != 0 && desc->desc != 0)
	{
		m_packet_desc_map.insert(std::make_pair(desc->id, desc->desc));
		desc++;
	}

	//packet log dictionary
	char packet_log_dir[128] = { 0 };
	if (m_config->is_packet_log_enable())
	{
		strncpy(packet_log_dir, get_logfile_name(), 128);
		char* dot = strrchr(packet_log_dir, '.');
		if (dot == 0) return false;

		*dot = 0;

#ifdef CY_SYS_WINDOWS
		if (0 == CreateDirectory(packet_log_dir, NULL))
#else
		if (mkdir(packet_log_dir, 0755) != 0) 
#endif
		{
			//create packet log dir error
			return false;
		}
	}

	for (int32_t i = 0; i < thread_counts; i++)
	{
		char name[128] = { 0 };
		snprintf(name, 128, "ag_%d", i);
		m_thread_pool[i] = new AgentThread(i, name, m_config->is_packet_log_enable() ? packet_log_dir : 0, m_config);
	}
	m_thread_counts = thread_counts;
	CY_LOG(L_DEBUG, "%d agent thread(s) started, encrypt %s", thread_counts, m_config ->is_packet_encrypt_enable()? "enable" : "disable");

	return true;
}

//-------------------------------------------------------------------------------------
const char* AgentService::get_packet_desc(int32_t id) const
{
	PacketDescMap::const_iterator it = m_packet_desc_map.find(id);
	if (it == m_packet_desc_map.end()) return 0;

	return it->second;
}

//-------------------------------------------------------------------------------------
int32_t AgentService::get_next_thread(void) {
	return (m_next_thread_id++) % m_thread_counts;
}

//-------------------------------------------------------------------------------------
void AgentService::send_message(int32_t thread_index, const Packet* message)
{
	assert(thread_index>=0 && thread_index < m_thread_counts);

	m_thread_pool[thread_index]->send_message(message);
}

//-------------------------------------------------------------------------------------
int32_t AgentService::get_next_agent_id(void)
{
	return m_next_agent_id++;
}

//-------------------------------------------------------------------------------------
void AgentService::debug(void)
{
	if (!Debug::getSingleton()->is_enable()) return;

	DebugCommand debugCmd;
	for (int32_t i = 0; i < m_thread_counts; i++) {

		Packet msg;
		msg.build(WorkThread::MESSAGE_HEAD_SIZE, DebugCommand::ID, sizeof(debugCmd), (const char*)&debugCmd);
		send_message(i, &msg);
	}
}
