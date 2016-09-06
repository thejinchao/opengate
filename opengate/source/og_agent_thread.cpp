#include "og_stdafx.h"
#include "og_agent_thread.h"
#include "og_message_define.h"
#include "og_agent.h"
#include "og_down_service.h"
#include "og_agent_service.h"
#include "og_up_service.h"
#include "og_up_thread.h"
#include "og_down_service.h"
#include "og_debug.h"

//-------------------------------------------------------------------------------------
AgentThread::AgentThread(int32_t index, const char* name, const char* packet_log_dir, Config* config)
	: m_thread_index(index)
	, m_config(config)
{
	m_packet_log_dir = packet_log_dir ? packet_log_dir : "";

	m_message_signal = sys_api::signal_create();
	//run work thread
	m_thread = sys_api::thread_create(_work_thread_entry, this, name);
}

//-------------------------------------------------------------------------------------
AgentThread::~AgentThread()
{
	//TODO: stop thread
}

//-------------------------------------------------------------------------------------
bool AgentThread::is_in_workthread(void) const
{
	return sys_api::thread_get_current_id() == sys_api::thread_get_id(m_thread);
}

//-------------------------------------------------------------------------------------
void AgentThread::work_thread_func(void)
{
	CY_LOG(L_DEBUG, "Work thread \"%s\" start...", sys_api::thread_get_current_name());
	srand((uint32_t)time(0));

	//alloc forword down message queue array
	m_down_stream_counts = m_config->get_down_stream().thread_counts;
	m_forword_down_batch = new MessageVector[m_down_stream_counts];

	for (size_t i = 0; i < m_down_stream_counts; i++) {
		MessageVector& batch = m_forword_down_batch[i];
		batch.reserve(MAX_FORWORD_DOWN_MSG_COUNTS);
	}

	for (;;)
	{
		//wait message
		sys_api::signal_wait(m_message_signal);

		for (;;) {
			Packet* packet;
			if (!m_message_queue.pop(packet)) break;

			//process the packet
			_on_message(packet);
		}

		_forword_message_down_end();
	}
}

//-------------------------------------------------------------------------------------
void AgentThread::_on_message(Packet* message)
{
	assert(is_in_workthread());
	assert(message);

	uint16_t msg_id = message->get_packet_id();

	switch (msg_id)
	{
	case RequestNewAgent::ID:	//DownService -> AgentService
		_create_agent(message);
		Packet::free_packet(message);
		break;

	case ReplyNewAgent::ID:	//UpService -> AgentService
		_on_up_connect_result(message);
		Packet::free_packet(message);
		break;

	case ForwordMessageUp::ID:
		_forword_message_up(message);
		Packet::free_packet(message);
		break;

	case ForwordMessageDown::ID:
		_forword_message_down(message);
		break;

	case DeleteAgentUp::ID:
		_delete_agent_up(message);
		Packet::free_packet(message);
		break;

	case DeleteAgentDown::ID:
		_delete_agent_down(message);
		Packet::free_packet(message);
		break;

	case DebugCommand::ID:
		_debug(message);
		Packet::free_packet(message);
		break;
	}
}

//-------------------------------------------------------------------------------------
void AgentThread::send_message(const Packet* message)
{
	Packet* packet = Packet::alloc_packet(message);
	if (!m_message_queue.push(packet)) {
		//WTF!!!
		CY_LOG(L_ERROR, "work thread message queue full!");
		return;
	}

	sys_api::signal_notify(m_message_signal);
}

//-------------------------------------------------------------------------------------
void AgentThread::_create_agent(Packet* message)
{
	assert(message->get_packet_size() == sizeof(RequestNewAgent));
	RequestNewAgent* requestNewAgent = (RequestNewAgent*)(message->get_packet_content());
	assert(requestNewAgent->agent_thread_index == get_index());

	//create new agent
	int32_t agent_id = requestNewAgent->agent_id;
	char packet_log_file[256] = { 0 };
	if (!m_packet_log_dir.empty()) {
		snprintf(packet_log_file, 256, "%s/agent%03d.log", m_packet_log_dir.c_str(), agent_id);
	}
	Agent* agent = new Agent(requestNewAgent->down_thread_index, 
		requestNewAgent->connection_id, get_index(), agent_id, UpService::getSingleton()->get_next_thread(), 
		m_packet_log_dir.empty() ? 0 : packet_log_file, m_config);

	m_agent_map.insert(std::make_pair(agent->get_agent_id(), agent));

	//dhkey exchange
	if (m_config->is_packet_encrypt_enable()) {
		dhkey_t public_key;
		public_key.dq.high = requestNewAgent->public_key.dq.high;
		public_key.dq.low = requestNewAgent->public_key.dq.low;
		dhkey_t my_public_key;
		agent->key_exchange(public_key, my_public_key);

		char debug_out[128] = { 0 };
		const dhkey_t& seckey = agent->get_secret_key();
		for (int i = 0; i < DH_KEY_LENGTH; i++)
		{
			snprintf(debug_out + i * 2, 4, "%02x", seckey.bytes[i]);
		}
		CY_LOG(L_DEBUG, "Agent_%d new Agent and key exchanged, sec key=%s", agent_id, debug_out);
	}

	//send create agent request to up service
	{
		RequestNewAgent newAgent;
		newAgent.connection_id = requestNewAgent->connection_id;
		newAgent.down_thread_index = requestNewAgent->down_thread_index;

		newAgent.agent_thread_index = this->get_index();
		newAgent.agent_id = agent_id;

		Packet packet;
		packet.build(WorkThread::MESSAGE_HEAD_SIZE, RequestNewAgent::ID, sizeof(RequestNewAgent), (const char*)&newAgent);
		UpService::getSingleton()->pick_stream(agent->get_up_thread_index())->send_message(&packet);
	}
}

//-------------------------------------------------------------------------------------
void AgentThread::_on_up_connect_result(Packet* message)
{
	assert(message->get_packet_size() == sizeof(ReplyNewAgent));

	ReplyNewAgent* result = (ReplyNewAgent*)(message->get_packet_content());
	assert(result->agent_thread_index == get_index());

	//select the agent
	AgentMap::iterator it = m_agent_map.find(result->agent_id);
	if (it == m_agent_map.end()) {
		//agent is not avaliable...
		CY_LOG(L_DEBUG, "Agent_%d agent is not avaliable when up connection result return", result->agent_id);

		//TODO: delete self and send del agent message to up service
		return;
	}
	Agent* agent = it->second;
	//check 
	assert(agent->get_agent_id() == result->agent_id);
	assert(agent->get_agent_thread_index() == result->agent_thread_index);
	assert(agent->get_up_thread_index() == result->up_thread_index);

	//set agent status
	agent->on_up_server_connected();

	//send result to down service
	{
		ReplyNewAgent replyNewAgent;
		replyNewAgent.down_thread_index = agent->get_down_thread_index();
		replyNewAgent.connection_id = agent->get_connection_id();
		replyNewAgent.agent_thread_index = this->get_index();
		replyNewAgent.agent_id = agent->get_agent_id();
		if (m_config->is_packet_encrypt_enable()) {
			agent->get_public_key(replyNewAgent.public_key);
		}else{
			replyNewAgent.public_key.dq.low = 0;
			replyNewAgent.public_key.dq.high = 0;
		}
		Packet packet;
		packet.build(WorkThread::MESSAGE_HEAD_SIZE, ReplyNewAgent::ID, sizeof(ReplyNewAgent), (const char*)&replyNewAgent);
		DownService::getSingleton()->send_message(agent->get_down_thread_index(), &packet);
	}

	CY_LOG(L_DEBUG, "Agent_%d up stream created!", agent->get_agent_id());
}

//-------------------------------------------------------------------------------------
void AgentThread::_forword_message_up(Packet* message)
{
	assert(message->get_packet_size() == sizeof(ForwordMessageUp));

	ForwordMessageUp* forword = (ForwordMessageUp*)(message->get_packet_content());
	assert(forword->agent_thread_index == get_index());

	//is agent exist
	AgentMap::iterator it = m_agent_map.find(forword->agent_id);
	if (it == m_agent_map.end()) {
		//error
		CY_LOG(L_WARN, "Agent_%d is not avaliable when forword msg from down to up", forword->agent_id);
		Packet::free_packet(forword->packet);
		return;
	}
	Agent* agent = it->second;

	//check agent up state
	if (!(agent->is_up_server_valid()))
	{
		CY_LOG(L_ERROR, "Agent_%d up is not avaliable when forword msg from down to up", forword->agent_id);
		Packet::free_packet(forword->packet);
		return;
	}

	//decrypt
	uint8_t* game_message = (uint8_t*)(forword->packet->get_packet_content());
	size_t game_message_len = forword->packet->get_packet_size();
	agent->decrypt_packet(game_message, game_message_len);

	//log packet
	agent->log_packet(message);

	//forwod to up service
	forword->up_thread_index = agent->get_up_thread_index();

	UpThread* upThread = UpService::getSingleton()->pick_stream(agent->get_up_thread_index());
	assert(upThread);

	upThread->send_message(message);
}

//-------------------------------------------------------------------------------------
void AgentThread::_forword_message_down_end(void)
{
	for (size_t i = 0; i < m_down_stream_counts; i++) {
		MessageVector& batch = m_forword_down_batch[i];
		if (batch.empty()) continue;

		DownService::getSingleton()->send_message((int32_t)i, (const Packet**)&(batch[0]), (int32_t)batch.size());

		for (size_t j = 0; j < batch.size(); j++) {
			Packet::free_packet(batch[j]);
		}
		batch.clear();
	}
}

//-------------------------------------------------------------------------------------
void AgentThread::_forword_message_down(Packet* message)
{
	assert(message->get_packet_size() == sizeof(ForwordMessageDown));

	ForwordMessageDown* forword = (ForwordMessageDown*)(message->get_packet_content());
	assert(forword->agent_thread_index == get_index());

	//is agent exist
	AgentMap::iterator it = m_agent_map.find(forword->agent_id);
	if (it == m_agent_map.end()) {
		//warning,  don't write log in formal enviroment, it happens offen
		//CY_LOG(L_WARN, "Agent_%d is not avaliable when forword msg from up to down", forword->agent_id);
		Packet::free_packet(forword->packet);
		return;
	}
	Agent* agent = it->second;

	//TODO: check agent down state

	//log packet
	agent->log_packet(message);

	//encrypt
	uint8_t* game_message = (uint8_t*)(forword->packet->get_packet_content());
	size_t game_message_len = forword->packet->get_packet_size();
	agent->encrypt_packet(game_message, game_message_len);

	//forwod to up service
	forword->down_thread_index = agent->get_down_thread_index();
	forword->connection_id = agent->get_connection_id();

	MessageVector& batch = m_forword_down_batch[agent->get_down_thread_index()];
	batch.push_back(message);
	if (batch.size() >= MAX_FORWORD_DOWN_MSG_COUNTS) {
		DownService::getSingleton()->send_message(agent->get_down_thread_index(), (const Packet**)&(batch[0]), (int32_t)batch.size());
	
		for (size_t j = 0; j < batch.size(); j++) {
			Packet::free_packet(batch[j]);
		}
		batch.clear();
	}
}

//-------------------------------------------------------------------------------------
void AgentThread::_delete_agent_up(Packet* message)
{	
	assert(message->get_packet_size() == sizeof(DeleteAgentUp));

	DeleteAgentUp* del = (DeleteAgentUp*)(message->get_packet_content());
	assert(del->agent_thread_index == get_index());

	CY_LOG(L_DEBUG, "Agent_%d receive del agent up cmd", del->agent_id);

	//is agent exist
	AgentMap::iterator it = m_agent_map.find(del->agent_id);
	if (it == m_agent_map.end()) {
		//error
		CY_LOG(L_WARN, "Agent_%d is not avaliable when delete agent msg from down to up", del->agent_id);
		return;
	}
	Agent* agent = it->second;

	//send to up service
	if (agent->get_up_thread_index() >= 0 )
	{
		del->up_thread_index = agent->get_up_thread_index();

		UpThread* upThread = UpService::getSingleton()->pick_stream(agent->get_up_thread_index());
		assert(upThread);

		CY_LOG(L_DEBUG, "Agent_%d send del agent cmd to up, thread=%d", del->agent_id, upThread->get_index());

		upThread->send_message(message);
	}

	//release self
	CY_LOG(L_DEBUG, "Agent_%d del agent", del->agent_id);

	//delete debug value
	agent->del_debug_value(Debug::getSingleton());

	m_agent_map.erase(it);
	delete agent;
}

//-------------------------------------------------------------------------------------
void AgentThread::_delete_agent_down(Packet* message)
{
	assert(message->get_packet_size() == sizeof(DeleteAgentDown));

	DeleteAgentDown* del = (DeleteAgentDown*)(message->get_packet_content());
	if (del->agent_thread_index == -1 && del->agent_id == -1) {
		//close all agent 
		_delete_all_agent();
		return;
	}
	assert(del->agent_thread_index == get_index());

	//is agent exist
	AgentMap::iterator it = m_agent_map.find(del->agent_id);
	if (it == m_agent_map.end()) {
		//error
		CY_LOG(L_WARN, "Agent_%d is not avaliable when delete agent msg from up to down", del->agent_id);
		return;
	}
	Agent* agent = it->second;

	//send to down service
	del->down_thread_index = agent->get_down_thread_index();
	del->connection_id = agent->get_connection_id();
	DownService::getSingleton()->send_message(agent->get_down_thread_index(), message);

	//delete debug value
	agent->del_debug_value(Debug::getSingleton());

	//release self
	m_agent_map.erase(it);
	delete agent;

	CY_LOG(L_DEBUG, "Agent_%d delete agent", del->agent_id);
}

//-------------------------------------------------------------------------------------
void AgentThread::_delete_all_agent(void)
{
	CY_LOG(L_DEBUG, "Delete all agent(%d)!", m_agent_map.size());
	for (AgentMap::iterator it = m_agent_map.begin(); it != m_agent_map.end(); ++it) {
		Agent* agent = it->second;
		
		//delete debug value
		agent->del_debug_value(Debug::getSingleton());

		delete agent;
	}
	m_agent_map.clear();
}

//-------------------------------------------------------------------------------------
void AgentThread::_debug(Packet*)
{
	if (!Debug::getSingleton()->is_enable()) return;

	char key_temp[MAX_PATH] = { 0 };

	snprintf(key_temp, MAX_PATH, "AgentThread:%d:agent_counts", m_thread_index);
	Debug::getSingleton()->set_debug_value(key_temp, (int32_t)m_agent_map.size());

	AgentMap::iterator it, end = m_agent_map.end();
	for (it = m_agent_map.begin(); it != m_agent_map.end(); ++it) {
		Agent* agent = it->second;

		agent->debug(Debug::getSingleton());
	}
}
