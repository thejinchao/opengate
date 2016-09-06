#include "og_stdafx.h"
#include "og_down_service.h"
#include "og_config.h"
#include "og_message_define.h"
#include "og_agent_service.h"
#include "og_debug.h"

#include "GateMessage.pb.h"
#include "HOpCode.h"

//-------------------------------------------------------------------------------------
DownService* DownService::s_singleton = 0;

//-------------------------------------------------------------------------------------
DownService::DownService()
	: m_server(0)
	, m_config(0)
	, m_work_threads(0)
{
	s_singleton = this;
}

//-------------------------------------------------------------------------------------
DownService::~DownService()
{
	s_singleton = 0;

	if (m_server) {
		delete m_server;
	}
	if (m_work_threads) {
		delete[] m_work_threads;
	}
	m_server = 0;
}

//-------------------------------------------------------------------------------------
bool DownService::start(const Config* config, DebugInterface* debuger)
{
	assert(config);

	m_config = config;
	const Config::DownStream& downStreamConfig = config->get_down_stream();

	m_work_thread_counts = downStreamConfig.thread_counts;
	m_work_threads = new ServerThread[m_work_thread_counts];
	for (int32_t i = 0; i < m_work_thread_counts; i++) {
		m_work_threads[i].thread_index = i;
	}

	Address address((uint16_t)downStreamConfig.server_port, false);

	m_server = new TcpServer(this, "dw", debuger);

	if (!(m_server->bind(address, true)))
		return false;

	if (!(m_server->start(downStreamConfig.thread_counts)))
		return false;

	return true;
}

//-------------------------------------------------------------------------------------
int32_t DownService::get_thread_counts(void) const
{
	return m_server ? m_server->get_work_thread_counts() : 0;
}

//-------------------------------------------------------------------------------------
void DownService::join(void)
{
	assert(m_server);
	m_server->join();
}

//-------------------------------------------------------------------------------------
void DownService::send_message(int32_t thread_index, const Packet* message)
{
	assert(m_server);
	m_server->send_work_message(thread_index, message);
}

//-------------------------------------------------------------------------------------
void DownService::send_message(int32_t thread_index, const Packet** message, int32_t counts)
{
	assert(m_server);
	m_server->send_work_message(thread_index, message, counts);
}

//-------------------------------------------------------------------------------------
void DownService::on_connection_callback(TcpServer*, int32_t thread_index, Connection* conn)
{
	assert(thread_index >= 0 && thread_index<m_work_thread_counts);

	CY_LOG(L_DEBUG, "Client connect from %s:%d success!",
		conn->get_peer_addr().get_ip(), conn->get_peer_addr().get_port());

	//create connection proxy
	ConnectionProxy* proxy = new ConnectionProxy;
	proxy->conn = conn;
	proxy->state = ConnectionProxy::kCreated;
	proxy->agent_thread_index = AgentService::getSingleton()->get_next_thread();
	proxy->agent_id = AgentService::getSingleton()->get_next_agent_id();
	conn->set_proxy(proxy);

	//set connection name
	char conn_name[32] = { 0 };
	snprintf(conn_name, 32, "agent%d", proxy->agent_id);
	conn->set_name(conn_name);

	//insert connection map
	m_work_threads[thread_index].connectionMap.insert(std::make_pair(conn->get_id(), proxy));
}

//-------------------------------------------------------------------------------------
void DownService::on_message_callback(TcpServer* server, int32_t thread_index, Connection* conn)
{
	(void)server;
	(void)thread_index;

	ConnectionProxy* proxy = (ConnectionProxy*)(conn->get_proxy());
	if (proxy == 0) {
		CY_LOG(L_WARN, "Connection already be delete when receive message");
		return;
	}

	//if agent is closing DO NOT process any packet
	if (proxy->state == ConnectionProxy::kClosing)
	{
		//up stream not connected
		CY_LOG(L_WARN, "Agent_%d receive down message when closing", proxy->agent_id);
		return;
	}

	//parser message
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

			if (!_process_gate_message(gate_message, thread_index, conn))
			{
				//error, kick off
				server->shutdown_connection(conn);
				return;
			}

			//OK, continue to other message
			continue;
		}
		else
		{
			Packet* game_message = Packet::alloc_packet();
			if (!(game_message->build(GAME_MESSAGE_HEAD_SIZE, conn->get_input_buf()))) {
				Packet::free_packet(game_message);
				break;
			}

			if (proxy->state != ConnectionProxy::kHandShaked)
			{
				//error state, kick off
				CY_LOG(L_ERROR, "Agent_%d is not connected when message receive from down stream, state=%d",
					proxy->agent_id, proxy->state);

				server->shutdown_connection(conn);
				Packet::free_packet(game_message);
				return;
			}

			//forwork to up stream
			ForwordMessageUp data;
			data.agent_thread_index = proxy->agent_thread_index;
			data.agent_id = proxy->agent_id;
			data.up_thread_index = -1;
			data.packet = game_message;

			Packet forword_message;
			forword_message.build(WorkThread::MESSAGE_HEAD_SIZE, ForwordMessageUp::ID, 
				(uint16_t)sizeof(ForwordMessageUp), (const char*)&data);

			AgentService::getSingleton()->send_message(proxy->agent_thread_index, &forword_message);
		}
	}
}

//-------------------------------------------------------------------------------------
void DownService::on_close_callback(TcpServer* server, int32_t thread_index, Connection* conn)
{
	(void)server;
	(void)thread_index;

	//update states
	ConnectionProxy* proxy = (ConnectionProxy*)(conn->get_proxy());
	if (proxy == 0) {
		CY_LOG(L_WARN, "Connection already be delete when socket closed");
		return;
	}

	CY_LOG(L_DEBUG, "Agent_%d socket closed, state=%d", proxy->agent_id, proxy->state);


	if (proxy->state != ConnectionProxy::kCreated &&
		proxy->state != ConnectionProxy::kClosing)
	{
		proxy->state = ConnectionProxy::kClosing;

		//send message to agent service
		DeleteAgentUp data;
		data.agent_thread_index = proxy->agent_thread_index;
		data.agent_id = proxy->agent_id;
		
		data.up_thread_index = -1;

		Packet message;
		message.build(WorkThread::MESSAGE_HEAD_SIZE, DeleteAgentUp::ID, sizeof(data), (const char*)&data);

		AgentService::getSingleton()->send_message(data.agent_thread_index, &message);

		CY_LOG(L_DEBUG, "Agent_%d send del agent cmd to agent service", proxy->agent_id);
	}

	//remove from connection map
	m_work_threads[thread_index].connectionMap.erase(conn->get_id());

	//release memory
	conn->set_proxy(0);
	delete proxy;
}

//-------------------------------------------------------------------------------------
void DownService::on_extra_workthread_msg(TcpServer* server, int32_t thread_index, Packet* msg)
{
	(void)server;

	//in work thread
	uint16_t msg_id = msg->get_packet_id();
	switch (msg_id)
	{
	case ReplyNewAgent::ID:
		_on_replay_new_agent(thread_index, msg);
		break;

	case ForwordMessageDown::ID:
		_forword_message_down(thread_index, msg);
		break;

	case DeleteAgentDown::ID:
		_on_del_agent(thread_index, msg);
		break;
	}
}

//-------------------------------------------------------------------------------------
void DownService::_on_replay_new_agent(int32_t thread_index, Packet* packet)
{
	assert(packet->get_packet_size() == sizeof(ReplyNewAgent));
	ReplyNewAgent* replyNewAgent = (ReplyNewAgent*)(packet->get_packet_content());
	assert(thread_index == replyNewAgent->down_thread_index);

	Connection* conn = m_server->get_connection(thread_index, replyNewAgent->connection_id);
	if (conn == 0) {
		//error
		CY_LOG(L_WARN, "Agent_%d Connection not avaiable when agent created", replyNewAgent->agent_id);
		//TODO: send delete agent up message to agent service
		return;
	}

	ConnectionProxy* proxy = (ConnectionProxy*)(conn->get_proxy());
	if (proxy == 0) {
		CY_LOG(L_WARN, "Agent_%d Connection already be delete when new agent reply returned", replyNewAgent->agent_id);
		return;
	}
	assert(proxy->state == ConnectionProxy::kHandShaking);
	proxy->state = ConnectionProxy::kHandShaked;
	proxy->agent_thread_index = replyNewAgent->agent_thread_index;
	proxy->agent_id = replyNewAgent->agent_id;

	CY_LOG(L_DEBUG, "Agent_%d receive create agent reply, connection id=%d[%p]", proxy->agent_id, conn->get_id(), conn);

	//send handshake to client
	GCHandshake handshake;
	handshake.set_result(0);	//OK!
	handshake.set_dhkey_low(replyNewAgent->public_key.dq.low);
	handshake.set_dhkey_high(replyNewAgent->public_key.dq.high);
	handshake.set_agent_id(replyNewAgent->agent_id);

	Packet reply_msg;
	reply_msg.build(GAME_MESSAGE_HEAD_SIZE, protobuf::message::GCHandshake, (uint16_t)handshake.ByteSize(), 0);
	handshake.SerializeToArray(reply_msg.get_memory_buf() + GAME_MESSAGE_HEAD_SIZE, reply_msg.get_packet_size());
	conn->send(reply_msg.get_memory_buf(), reply_msg.get_memory_size());
}

//-------------------------------------------------------------------------------------
void DownService::_forword_message_down(int32_t thread_index, Packet* packet)
{
	assert(packet->get_packet_size() == sizeof(ForwordMessageDown));
	ForwordMessageDown* reply = (ForwordMessageDown*)(packet->get_packet_content());
	assert(thread_index == reply->down_thread_index);

	Packet* game_packet = reply->packet;
	Connection* conn = m_server->get_connection(thread_index, reply->connection_id);
	if (conn == 0 || conn->get_state()!=Connection::kConnected) {
		//warning, don't write log in formal enviroment, it happens offen
		//CY_LOG(L_WARN, "Agent_%d Connection not avaiable when forword msg from up to down, state=%d", reply->agent_id, (conn?conn->get_state():-1));
		Packet::free_packet(game_packet);
		return;
	}

	conn->send(game_packet->get_memory_buf(), game_packet->get_memory_size());

	Packet::free_packet(game_packet);	//alloced in up thread!
	reply->packet = 0;
}

//-------------------------------------------------------------------------------------
void DownService::_delete_all_agent(int32_t thread_index)
{
	assert(thread_index >= 0 && thread_index<m_work_thread_counts);

	ConnectionProxyMap& connectionMap = m_work_threads[thread_index].connectionMap;

	CY_LOG(L_DEBUG, "Close all connection(%d)", connectionMap.size());
	for (ConnectionProxyMap::iterator it = connectionMap.begin(); it != connectionMap.end(); ++it)
	{
		ConnectionProxy* proxy = it->second;

		proxy->state = ConnectionProxy::kClosing;
		m_server->shutdown_connection(proxy->conn);
	}
}

//-------------------------------------------------------------------------------------
void DownService::_on_del_agent(int32_t thread_index, Packet* packet)
{
	assert(packet->get_packet_size() == sizeof(DeleteAgentDown));
	DeleteAgentDown* del = (DeleteAgentDown*)(packet->get_packet_content());

	if (del->down_thread_index == -1 && del->connection_id == -1) {
		_delete_all_agent(thread_index);
		return;
	}

	assert(thread_index == del->down_thread_index);

	Connection* conn = m_server->get_connection(thread_index, del->connection_id);
	if (conn == 0) {
		//error
		CY_LOG(L_WARN, "Agent_%d Connection not avaiable when del agent msg from up to down", del->agent_id);
		return;
	}

	ConnectionProxy* proxy = (ConnectionProxy*)(conn->get_proxy());
	if (proxy == 0) {
		CY_LOG(L_WARN, "Agent_%d Connection already be delete when delete agent received!", del->agent_id);
		return;
	}

	CY_LOG(L_DEBUG, "Agent_%d receive del agent msg from agent service", del->agent_id);
	proxy->state = ConnectionProxy::kClosing;
	m_server->shutdown_connection(conn);
}

//-------------------------------------------------------------------------------------
void DownService::_signal_terminate(void)
{
	CY_LOG(L_DEBUG, "receive terminate signal, system quit!");
	m_server->stop();
}

//-------------------------------------------------------------------------------------
void DownService::debug(void)
{
	m_server->debug();
}