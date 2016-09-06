#include "og_stdafx.h"
#include "og_up_service.h"
#include "og_up_thread.h"
#include "og_config.h"
#include "og_message_define.h"
#include "og_debug.h"

//-------------------------------------------------------------------------------------
UpService* UpService::s_singleton = 0;

//-------------------------------------------------------------------------------------
UpService::UpService()
	: m_threads(0)
	, m_thread_counts(0)
{
	s_singleton = this;
}

//-------------------------------------------------------------------------------------
UpService::~UpService()
{
	s_singleton = 0;
}

//-------------------------------------------------------------------------------------
bool UpService::start(const Config* config)
{
	const Config::UpStream& upstream = config->get_up_stream();


	m_thread_counts = upstream.thread_counts;
	m_threads = new UpThread*[m_thread_counts];

	m_next_thread = 0;
	m_up_connection_counts = 0;
	m_handshake_version = 0;

	for (uint32_t j = 0; j < m_thread_counts; j++)
	{
		char name[32] = { 0 };
		snprintf(name, 32, "up_%d", j);
		m_threads[j] = new UpThread(&upstream, j, name);
	}
	CY_LOG(L_DEBUG, "%d up thread(s) for up server started.", m_thread_counts);

	return true;
}

//-------------------------------------------------------------------------------------
UpThread* UpService::pick_stream(int32_t thread_index) const
{
	assert(thread_index>=0 && (size_t)thread_index < m_thread_counts);

	return m_threads[thread_index];
}

//-------------------------------------------------------------------------------------
void UpService::on_up_connection(bool conn, int32_t handshake_version)
{
	if (conn) {
		m_up_connection_counts++;
		m_handshake_version = handshake_version;
	}
	else {
		m_up_connection_counts--;

		//TODO: restart connect timer
		if (m_up_connection_counts == 0) 
		{
			ReconnectUpStream restart;
			restart.sleep_time = 5; //reconnect to up stream after 5 seconds

			Packet message;
			message.build(WorkThread::MESSAGE_HEAD_SIZE, ReconnectUpStream::ID, sizeof(restart), (const char*)&restart);

			for (uint32_t i = 0; i < m_thread_counts; i++)
			{
				m_threads[i]->send_message(&message);
			}
		}
	}
}

//-------------------------------------------------------------------------------------
void UpService::debug(void)
{
	if (!Debug::getSingleton()->is_enable()) return;

	DebugCommand debugCmd;
	for (size_t i = 0; i < m_thread_counts; i++) {

		Packet msg;
		msg.build(WorkThread::MESSAGE_HEAD_SIZE, DebugCommand::ID, sizeof(debugCmd), (const char*)&debugCmd);
		m_threads[i]->send_message(&msg);
	}
}
