#include "og_stdafx.h"
#include "og_agent.h"
#include "og_message_define.h"
#include "og_agent_service.h"
#include "og_config.h"

//-------------------------------------------------------------------------------------
Agent::Agent(int32_t down_thread_index, int32_t connection_id,
			int32_t agent_thread_index, int32_t agent_id,
			int32_t up_thread_index,
			const char* packet_log_file, 
			Config* config)
	: m_down_thread_index(down_thread_index)
	, m_connection_id(connection_id)
	, m_agent_thread_index(agent_thread_index)
	, m_agent_id(agent_id)
	, m_up_valid(false)
	, m_up_thread_index(up_thread_index)
	, m_packet_log_file(packet_log_file ? packet_log_file : "")
	, m_total_down_size(0)
	, m_total_up_size(0)
	, m_config(config)
{
	if (!m_packet_log_file.empty()) {
		FILE* fp = fopen(m_packet_log_file.c_str(), "w");
		fclose(fp);
	}
}

//-------------------------------------------------------------------------------------
Agent::~Agent()
{
}

//-------------------------------------------------------------------------------------
void Agent::key_exchange(const dhkey_t& another_pub_key, dhkey_t& public_key)
{
	//generate self key pair
	dhkey_t private_key;
	DH_generate_key_pair(public_key, private_key);

	//exchange
	DH_generate_key_secret(m_secret_key, private_key, another_pub_key);

	//destroy private key
	private_key.dq.high = private_key.dq.low = 0;
	
	//store public key
	m_public_key.dq.low = public_key.dq.low;
	m_public_key.dq.high = public_key.dq.high;

	//init encrypt/decrypt
	m_decrypt.seed0 = m_secret_key.dq.low;
	m_decrypt.seed1 = m_secret_key.dq.high;

	m_encrypt.seed0 = m_secret_key.dq.high;
	m_encrypt.seed1 = m_secret_key.dq.low;
}

//-------------------------------------------------------------------------------------
void Agent::log_packet(Packet* packet)
{
	if (m_packet_log_file.empty()) return;
	FILE* fp = fopen(m_packet_log_file.c_str(), "a");
	if (fp == 0) return;

	char timebuf[32] = { 0 };
#ifdef CY_SYS_WINDOWS
	//current time
	SYSTEMTIME time;
	::GetLocalTime(&time);

	snprintf(timebuf, sizeof(timebuf), "%04d_%02d_%02d-%02d:%02d:%02d",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
#else
	time_t t = time(0);
	struct tm tm_now;
	localtime_r(&t, &tm_now);

	strftime(timebuf, sizeof(timebuf), "%Y_%m_%d-%H:%M:%S", &tm_now);
#endif

	if (packet->get_packet_id() == ForwordMessageUp::ID) {
		assert(packet->get_packet_size() == sizeof(ForwordMessageUp));
		ForwordMessageUp* forword = (ForwordMessageUp*)(packet->get_packet_content());

		Packet* game_message = forword->packet;
		uint16_t size = game_message->get_packet_size();
		uint16_t id = game_message->get_packet_id();

		fprintf(fp, "%s\t%d\t->%s\n", timebuf, size, AgentService::getSingleton()->get_packet_desc(id));
		fclose(fp);
	}
	else if (packet->get_packet_id() == ForwordMessageDown::ID) {
		assert(packet->get_packet_size() == sizeof(ForwordMessageDown));
		ForwordMessageDown* forword = (ForwordMessageDown*)(packet->get_packet_content());

		Packet* game_message = forword->packet;
		uint16_t size = game_message->get_packet_size();
		uint16_t id = game_message->get_packet_id();

		fprintf(fp, "%s\t%d\t<-%s\n", timebuf, size, AgentService::getSingleton()->get_packet_desc(id));
		fclose(fp);
	}
}

//-------------------------------------------------------------------------------------
void Agent::decrypt_packet(uint8_t* message, size_t message_size)
{
	m_total_up_size += (int32_t)message_size;

	if (m_config->is_packet_encrypt_enable()) {

		xorshift128(message, message_size, m_decrypt);
	}
}

//-------------------------------------------------------------------------------------
void Agent::encrypt_packet(uint8_t* message, size_t message_size)
{
	m_total_down_size += (int32_t)message_size;

	if (m_config->is_packet_encrypt_enable()) {

		xorshift128(message, message_size, m_encrypt);
	}
}

//-------------------------------------------------------------------------------------
void Agent::debug(DebugInterface* debuger)
{
	if (!debuger || !(debuger->is_enable())) return;

	char key_temp[MAX_PATH] = { 0 };

	snprintf(key_temp, MAX_PATH, "Agent:agent%d:total_up_size", get_agent_id());
	debuger->set_debug_value(key_temp, get_total_up_size());

	snprintf(key_temp, MAX_PATH, "Agent:agent%d:total_down_size", get_agent_id());
	debuger->set_debug_value(key_temp, get_total_down_size());
}

//-------------------------------------------------------------------------------------
void Agent::del_debug_value(DebugInterface* debuger)
{
	if (!debuger || !(debuger->is_enable())) return;

	char key_temp[MAX_PATH] = { 0 };

	snprintf(key_temp, MAX_PATH, "Agent:agent%d:total_up_size", get_agent_id());
	debuger->del_debug_value(key_temp);

	snprintf(key_temp, MAX_PATH, "Agent:agent%d:total_down_size", get_agent_id());
	debuger->del_debug_value(key_temp);
}

