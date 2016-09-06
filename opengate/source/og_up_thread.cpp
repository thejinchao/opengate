#include "og_stdafx.h"
#include "og_up_thread.h"
#include "og_message_define.h"
#include "og_up_service.h"
#include "og_agent_service.h"
#include "og_util.h"
#include "og_down_service.h"
#include "og_debug.h"

#include "GateMessage.pb.h"
#include "HOpCode.h"

//-------------------------------------------------------------------------------------
UpThread::UpThread(const Config::UpStream* config, int32_t index, const char* name)
	: m_config(config)
	, m_thread_index(index)
	, m_client(0)
	, m_total_up_size(0)
	, m_total_down_size(0)
{
	//run work thread
	m_work_thread = new WorkThread();
	m_work_thread->start(name, this);
}

//-------------------------------------------------------------------------------------
UpThread::~UpThread()
{
	delete m_work_thread;
}

//-------------------------------------------------------------------------------------
bool UpThread::is_in_workthread(void) const
{
	return sys_api::thread_get_current_id() == m_work_thread->get_looper()->get_thread_id();
}

//-------------------------------------------------------------------------------------
void UpThread::send_message(Packet* message)
{
	assert(m_work_thread);

	m_work_thread->send_message(message);
}

//-------------------------------------------------------------------------------------
bool UpThread::on_workthread_start(void)
{
	CY_LOG(L_DEBUG, "Work thread \"%s\" start...", m_work_thread->get_name());

	//connect to up stream
	m_client = new TcpClient(m_work_thread->get_looper(), this, this);

	//begin connect
	Address address((const char*)(m_config->server_ip.c_str()), (uint16_t)(m_config->server_port));
	m_client->connect(address, 1000 * 3);

	CY_LOG(L_DEBUG, "Begin connect to %s:%d...",
		(const char*)(m_config->server_ip.c_str()), (uint16_t)(m_config->server_port));

	return true;
}

//-------------------------------------------------------------------------------------
bool UpThread::on_workthread_message(Packet* message)
{
	assert(is_in_workthread());
	assert(message);
	uint16_t msg_id = message->get_packet_id();

	if (m_client->get_connection_state() != Connection::kConnected && msg_id != ReconnectUpStream::ID) {
		//TODO: error
		return false;
	}


	switch (msg_id)
	{
	case RequestNewAgent::ID:
		_create_new_agent(message);
		break;

	case ForwordMessageUp::ID:
		_forword_message(message);
		break;

	case DeleteAgentUp::ID:
		_delete_agent(message);
		break;

	case ReconnectUpStream::ID:
		_restart_connect(message);
		break;

	case DebugCommand::ID:
		_debug(message);
		break;
	}

	return false;
}

//-------------------------------------------------------------------------------------
void UpThread::_create_new_agent(Packet* message)
{
	assert(message->get_packet_size() == sizeof(RequestNewAgent));
	RequestNewAgent* request = (RequestNewAgent*)(message->get_packet_content());

	//check client status
	if (m_client == 0 || m_client->get_connection_state() != Connection::kConnected)
	{
		CY_LOG(L_WARN, "Agent_%d Client connection not avaliable when new agent msg need forword to up, state=%d",
			request->agent_id, (m_client ? m_client->get_connection_state() : -1));
		return;
	}
	CY_LOG(L_DEBUG, "Agent_%d receive new agent cmd ", request->agent_id);

	//create client proxy in up thread
	ClientProxy* proxy = new ClientProxy;
	proxy->agent_thread_index = request->agent_thread_index;
	proxy->agent_id = request->agent_id;
	proxy->state = kCreated;

	m_clientMap.insert(std::make_pair(proxy->agent_id, proxy));

	//send CGHandshake to Up stream
	CGHandshake handshake;
	handshake.set_version(UpService::getSingleton()->get_handshake_version());

	char game_message[MAX_GATE_MESSAGE_LENGTH];
	size_t game_message_len = build_server_msg_from_protobuf(::protobuf::message::CGHandshake, proxy->agent_id, &handshake, game_message, MAX_GATE_MESSAGE_LENGTH);
	assert(game_message_len <= MAX_GATE_MESSAGE_LENGTH);

	//send message
	m_client->send(game_message, game_message_len);
}

//-------------------------------------------------------------------------------------
void UpThread::_forword_message(Packet* message)
{
	assert(message->get_packet_size() == sizeof(ForwordMessageUp));
	ForwordMessageUp* forword = (ForwordMessageUp*)(message->get_packet_content());
	Packet* game_message = forword->packet;

	TcpClientMap::iterator it = m_clientMap.find(forword->agent_id);
	if (it == m_clientMap.end()) {
		CY_LOG(L_DEBUG, "Agent_%d up client is not avaliable when forword msg from agent to up", forword->agent_id);
		Packet::free_packet(game_message);
		return;
	}

	//check client status
	if (m_client == 0 || m_client->get_connection_state() != Connection::kConnected)
	{
		CY_LOG(L_WARN, "Agent_%d client not avaliable when msg need forword to up", 
			forword->agent_id, (m_client ? m_client->get_connection_state() : -1));
		Packet::free_packet(game_message);
		return;
	}

	char* game_message_head = game_message->get_memory_buf();
	size_t game_message_len = game_message->get_memory_size();

	//set agent id
	*(uint32_t*)(game_message_head + 4) = socket_api::ntoh_32(forword->agent_id);

	//send message
	m_client->send(game_message->get_memory_buf(), game_message_len);

	Packet::free_packet(game_message);	//alloced in down thread
	forword->packet = 0;

	m_total_up_size += game_message_len;
}

//-------------------------------------------------------------------------------------
void UpThread::_delete_agent(Packet* message)
{
	assert(message->get_packet_size() == sizeof(DeleteAgentUp));
	DeleteAgentUp* del = (DeleteAgentUp*)(message->get_packet_content());

	CY_LOG(L_DEBUG, "Agent_%d receive be deleted cmd from agent service", del->agent_id);

	TcpClientMap::iterator it = m_clientMap.find(del->agent_id);
	if (it == m_clientMap.end()) {
		CY_LOG(L_WARN, "Agent_%d up client is not avaliable when del agent msg from agent to up", del->agent_id);
		return;
	}

	//set state closing
	ClientProxy* proxy = it->second;
	proxy->state = kClosing;

	//send agent leave message to up stream
	GateAgentLeave agentLeave;
	agentLeave.set_agent_id(del->agent_id);

	char game_message[MAX_GATE_MESSAGE_LENGTH];
	size_t game_message_len = build_server_msg_from_protobuf(::protobuf::message::GateAgentLeave, proxy->agent_id, &agentLeave, game_message, MAX_GATE_MESSAGE_LENGTH);
	assert(game_message_len <= MAX_GATE_MESSAGE_LENGTH);

	//send message
	m_client->send(game_message, game_message_len);

	//we don't use it again
	m_clientMap.erase(it);
	delete proxy; proxy = 0; 
}

//-------------------------------------------------------------------------------------
void UpThread::_restart_connect(Packet* message)
{
	assert(message->get_packet_size() == sizeof(ReconnectUpStream));
	ReconnectUpStream* restart = (ReconnectUpStream*)(message->get_packet_content());

	CY_LOG(L_DEBUG, "Reconnect to up stream after %d second(s)", restart->sleep_time);

	sys_api::thread_sleep(restart->sleep_time*1000);

	delete m_client; m_client = 0;
	on_workthread_start();
}

//-------------------------------------------------------------------------------------
uint32_t UpThread::on_connection_callback(TcpClient* client, bool success)
{
	if (!success) {
		//reconnect after 5 seconds
		return 5 * 1000;
	}
	CY_LOG(L_DEBUG, "Connect to %s:%d success.", client->get_server_address().get_ip(), client->get_server_address().get_port());
	return 0;
}

//-------------------------------------------------------------------------------------
void UpThread::on_message_callback(TcpClient* , Connection* conn)
{
	for (;;) {

		//peed message id
		uint16_t packetID;
		if (sizeof(packetID) != conn->get_input_buf().peek(2, &packetID, sizeof(packetID))) break;
		packetID = socket_api::ntoh_16(packetID);

		//is gate server message?
		if (packetID >= GATE_MESSAGE_ID_BEGIN && packetID < GATE_MESSAGE_ID_END)
		{
			Packet gate_message;
			if (!gate_message.build(GAME_MESSAGE_HEAD_SIZE, conn->get_input_buf())) break;

			_process_gate_message(&gate_message);
		}
		else
		{
			Packet* game_message = Packet::alloc_packet();
			if (!(game_message->build(GAME_MESSAGE_HEAD_SIZE, conn->get_input_buf()))) {
				Packet::free_packet(game_message);
				break;
			}

			uint32_t agent_id = socket_api::ntoh_32(*(uint32_t*)(game_message->get_memory_buf()+4));

			TcpClientMap::iterator it = m_clientMap.find(agent_id);
			if (it == m_clientMap.end()) {
				//It's not necessary to write this log
				//CY_LOG(L_ERROR, "Agent_%d Receive message[%d] which agent_id is not in clientmap", agent_id, packet_id);
				Packet::free_packet(game_message);
				continue;
			}
			ClientProxy* proxy = it->second;

			//forwork to down stream
			ForwordMessageDown data;
			data.agent_thread_index = proxy->agent_thread_index;
			data.agent_id = proxy->agent_id;
			data.down_thread_index = -1;
			data.connection_id = -1;
			data.packet = game_message;

			Packet forword_message;
			forword_message.build(WorkThread::MESSAGE_HEAD_SIZE, ForwordMessageDown::ID,
				(uint16_t)sizeof(ForwordMessageDown), (const char*)&data);

			m_total_down_size += game_message->get_memory_size();

			//send to down stream
			AgentService::getSingleton()->send_message(proxy->agent_thread_index, &forword_message);
		}
	}
}

//-------------------------------------------------------------------------------------
void UpThread::on_close_callback(TcpClient*)
{
	CY_LOG(L_DEBUG, "Up Stream close!");

	//send delete all agent to agent service
	DeleteAgentDown data;
	data.agent_thread_index = -1;
	data.agent_id = -1;
	data.down_thread_index = -1;
	data.connection_id = -1;

	Packet message;
	message.build(WorkThread::MESSAGE_HEAD_SIZE, DeleteAgentDown::ID, sizeof(DeleteAgentDown), (const char*)&data);

	//send to all agent thread
	for (int32_t i = 0; i < AgentService::getSingleton()->get_thread_counts(); i++) {
		AgentService::getSingleton()->send_message(i, &message);
	}

	//send to all up thread
	for (int32_t i = 0; i < DownService::getSingleton()->get_thread_counts(); i++) {
		DownService::getSingleton()->send_message(i, &message);
	}

	//release memory
	for (TcpClientMap::iterator it = m_clientMap.begin(); it != m_clientMap.end(); ++it) {
		delete it->second;
	}
	m_clientMap.clear();

	//notify service
	UpService::getSingleton()->on_up_connection(false, 0);
}

//-------------------------------------------------------------------------------------
void UpThread::_process_gate_message(const Packet* message)
{
	switch (message->get_packet_id()) 
	{
	case ::protobuf::message::GateConnected:
		{
			//receive connection confirm message from server
			GateConnected gateConnected;
			if (!gateConnected.ParseFromArray(message->get_packet_content(), message->get_packet_size()))
			{
				CY_LOG(L_ERROR, "Parse GateConnected msg failed!");
				break;
			}

			//notify up service
			CY_LOG(L_DEBUG, "Receive connect confirm message.");
			UpService::getSingleton()->on_up_connection(true, gateConnected.version());
		}
		break;

	case ::protobuf::message::GCHandshake:
		{
			GCHandshake handshake;
			if (!handshake.ParseFromArray(message->get_packet_content(), message->get_packet_size()))
			{
				CY_LOG(L_ERROR, "Parse GCHandshake msg failed!");
				break;
			}

			uint32_t agent_id = socket_api::ntoh_32(*(uint32_t*)(message->get_memory_buf() + 4));
			TcpClientMap::iterator it = m_clientMap.find(agent_id);
			if (it == m_clientMap.end()) {
				CY_LOG(L_ERROR, "Agent_%d Can't find client proxy when receive GCHandshake", agent_id);
				break;
			}

			//check state
			ClientProxy* client = it->second;
			if (client->state != kCreated) {
				CY_LOG(L_ERROR, "Agent_%d State wrong(%d) when receive GCHandshake", agent_id, client->state);
				break;
			}
			client->state = kReady;

			//send to agent service
			ReplyNewAgent reply;
			reply.up_thread_index = this->get_index();
			reply.agent_id = client->agent_id;
			reply.agent_thread_index = client->agent_thread_index;
			
			Packet message;
			message.build(WorkThread::MESSAGE_HEAD_SIZE, ReplyNewAgent::ID, sizeof(ReplyNewAgent), (const char*)&reply);
			AgentService::getSingleton()->send_message(client->agent_thread_index, &message);
		}
		break;

	case ::protobuf::message::GateKickoffAgent:
		{
			GateKickoffAgent kickoff;
			if (!kickoff.ParseFromArray(message->get_packet_content(), message->get_packet_size()))
			{
				CY_LOG(L_ERROR, "Parse GateKickoffAgent msg failed!");
				break;
			}

			uint32_t agent_id = socket_api::ntoh_32(*(uint32_t*)(message->get_memory_buf() + 4));
			assert(agent_id == kickoff.agent_id());

			TcpClientMap::iterator it = m_clientMap.find(agent_id);
			if (it == m_clientMap.end()) {
				CY_LOG(L_ERROR, "Agent_%d Can't find client proxy when receive GateKickoffAgent", agent_id);
				break;
			}

			//check state
			ClientProxy* client = it->second;

			client->state = kClosing;

			//send to agent service
			DeleteAgentDown reply;
			reply.agent_id = client->agent_id;
			reply.agent_thread_index = client->agent_thread_index;

			Packet message;
			message.build(WorkThread::MESSAGE_HEAD_SIZE, DeleteAgentDown::ID, sizeof(DeleteAgentDown), (const char*)&reply);
			AgentService::getSingleton()->send_message(client->agent_thread_index, &message);
	
			//delete client proxy
			delete client;
			m_clientMap.erase(it);
			CY_LOG(L_DEBUG, "Agent_%d Delete ClientProxy", agent_id);
	}
		break;
	}
}

//-------------------------------------------------------------------------------------
void UpThread::_debug(Packet*)
{
	if (!Debug::getSingleton()->is_enable()) return;

	char key_temp[MAX_PATH] = { 0 };
	char value_temp[MAX_PATH] = { 0 };

	snprintf(key_temp, MAX_PATH, "UpThread:%d:total_up_size", m_thread_index);
	snprintf(value_temp, MAX_PATH, "%ld", m_total_up_size);
	Debug::getSingleton()->set_debug_value(key_temp, value_temp);

	snprintf(key_temp, MAX_PATH, "UpThread:%d:total_down_size", m_thread_index);
	snprintf(value_temp, MAX_PATH, "%ld", m_total_down_size);
	Debug::getSingleton()->set_debug_value(key_temp, value_temp);
}
