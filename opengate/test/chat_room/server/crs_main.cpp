#include "cr_stdafx.h"
#include "cr_chat_package.h"

#include "GateMessage.pb.h"
#include "ChatMessage.pb.h"
#include "HOpCode.h"

#define CURRENT_HANDSHAKE_VERSION 5

class ServerListener : public TcpServer::Listener
{
	//-------------------------------------------------------------------------------------
	virtual void on_connection_callback(TcpServer*, int32_t thread_index, Connection* conn)
	{
		//new connection
		sys_api::auto_mutex lock(this->connections_lock);
		this->connections.insert(conn);

		//send gate connected message
		GateConnected gateConnected;
		gateConnected.set_version(CURRENT_HANDSHAKE_VERSION);

		ChatPackage package;
		package.buildFromProtobuf(::protobuf::message::GateConnected, 0, &gateConnected, 0);
		conn->send(package.get_buf(), package.get_buf_length());

		CY_LOG(L_DEBUG, "new connection accept, from %s:%d to %s:%d",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port(),
			conn->get_local_addr().get_ip(),
			conn->get_local_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	virtual void on_message_callback(TcpServer* server, int32_t thread_index, Connection* conn)
	{
		RingBuf& buf = conn->get_input_buf();

		for (;;)
		{
			ChatPackage packet;
			if (!packet.buildFromRingBuf(buf, 0)) return;


			if (packet.get_packet_id() == ::protobuf::message::CGHandshake) 
			{
				CGHandshake handshake;
				handshake.ParseFromArray(packet.get_content(), packet.get_packet_size());
				if (handshake.version() != CURRENT_HANDSHAKE_VERSION) {
					//TODO: ERROR!
					continue;
				}
				
				uint32_t agent_id = packet.get_agent_id();
				CY_LOG(L_DEBUG, "Recive CGHandshake from agent_%d, conn=%p[%d]", agent_id, conn, conn->get_id());

				//new client
				{
					sys_api::auto_mutex lock(this->clients_lock);

					Client client;
					client.agent_id = agent_id;
					client.connection = conn;
					this->clients.insert(std::make_pair(agent_id, client));
				}

				//send CGHandshake reply
				GCHandshake handshake_reply;
				handshake_reply.set_result(0);

				ChatPackage returnPacket;
				returnPacket.buildFromProtobuf(::protobuf::message::GCHandshake, agent_id, &handshake_reply, 0);
				conn->send(returnPacket.get_buf(), returnPacket.get_buf_length());
				continue;
			}
			else if (packet.get_packet_id() == ::protobuf::message::GateAgentLeave)
			{
				GateAgentLeave agentLeave;
				agentLeave.ParseFromArray(packet.get_content(), packet.get_packet_size());

				uint32_t agent_id = agentLeave.agent_id();
				{
					sys_api::auto_mutex lock(this->clients_lock);
					this->clients.erase(agent_id);
				}
				CY_LOG(L_DEBUG, "remove agent %d", agent_id);
			}
			else if (packet.get_packet_id() == ::protobuf::message::CGChat)
			{
				CGChat chatMessage;
				chatMessage.ParseFromArray(packet.get_content(), packet.get_packet_size());

				const char* message = chatMessage.content().c_str();
				int32_t agent_id = chatMessage.agentid();

				CY_LOG(L_DEBUG, "receive from agent %d : %s", agent_id, message);

				if (strcmp(message, "exit") == 0)
				{
					server->shutdown_connection(conn);
					return;
				}
				else if (strncmp(message, "kickoff", 7) == 0) {
					int id = atoi(message + 7);

					GateKickoffAgent kickoff;
					kickoff.set_agent_id(id);

					ChatPackage returnPacket;
					returnPacket.buildFromProtobuf(::protobuf::message::GateKickoffAgent, id, &kickoff, 0);
					{
						sys_api::auto_mutex lock(this->clients_lock);
						std::map< uint32_t, Client >::iterator it = this->clients.find(id);
						if (it != this->clients.end()) {
							it->second.connection->send(returnPacket.get_buf(), returnPacket.get_buf_length());
							this->clients.erase(it);
						}
					}
					continue;
				}

				char temp[1024] = { 0 };

				size_t len = packet.get_packet_size();
				for (size_t i = 0; i < len; i++) {
					if (this->work_mode == 1) {
						temp[i] = (char)toupper(message[i]);
					}
					else{
						temp[i] = (char)tolower(message[i]);
					}
				}

				ChatPackage returnPacket;
				returnPacket.buildFromProtobuf(::protobuf::message::CGChat, 0, &chatMessage, 0);

				std::map< uint32_t, Client >::iterator it;
				{
					sys_api::auto_mutex lock(this->clients_lock);
					for (it = this->clients.begin(); it != this->clients.end(); ++it)
					{
						const Client& client = it->second;
						returnPacket.set_agent_id(client.agent_id);

						client.connection->send(returnPacket.get_buf(), returnPacket.get_buf_length());
					}
				}
			}
		}
	}

	//-------------------------------------------------------------------------------------
	virtual void on_close_callback(TcpServer*, int32_t thread_index, Connection* conn)
	{
		sys_api::auto_mutex lock(this->connections_lock);

		this->connections.erase(conn);
		CY_LOG(L_DEBUG, "connection %s:%d closed",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	void on_extra_workthread_msg(TcpServer* server, int32_t thread_index, Packet* msg)
	{

	}


private:
	std::set< Connection* > connections;
	sys_api::mutex_t connections_lock;

	struct Client
	{
		Connection* connection;
		uint32_t agent_id;
	};
	std::map< uint32_t, Client > clients;
	sys_api::mutex_t clients_lock;

	int32_t work_mode;

public:
	ServerListener(int32_t work_mode)
	{
		this->work_mode = work_mode;
		this->connections_lock = sys_api::mutex_create();
		this->clients_lock = sys_api::mutex_create();
	}
};

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{	
	uint16_t server_port = 8301;
	int32_t work_mode = 1;

	if (argc > 1)
		server_port = (uint16_t)atoi(argv[1]);

	if (argc > 2)
		work_mode = (int32_t)atoi(argv[2]);

	CY_LOG(L_DEBUG, "listen port %d, workmode=%d", server_port, work_mode);

	char name[128] = { 0 };
	snprintf(name, 128, "chat_%d", work_mode);

	ServerListener listener(work_mode);
	TcpServer* server = new TcpServer(&listener, name, 0);
	server->bind(Address(server_port, false), true);

	if (!(server->start(2)))
		return 1;

	server->join();
	
	delete server;
	return 0;
}
