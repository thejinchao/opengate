#include "pr_stdafx.h"
#include "pr_package.h"

class ServerListener : public TcpServer::Listener
{
	//-------------------------------------------------------------------------------------
	virtual void on_connection_callback(TcpServer*, int32_t thread_index, Connection* conn)
	{
		sys_api::auto_mutex lock(this->clients_lock);
		this->clients.insert(conn);

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
			Package packet;
			if (!packet.buildFromBuf(buf)) return;
			if (packet.get_Packet_id() != 10001) return;

			const char* message = packet.get_content();
			size_t len = packet.get_packetsize();

			uint32_t adler1 = adler32(0, 0, 0);
			adler1 = adler32(adler1, message, len - 4);

			uint32_t adler2 = 0;
			memcpy(&adler2, message + len - 4, sizeof(adler2));
			if (adler1 != adler2) {
				CY_LOG(L_ERROR, "adler32 error! [%08x]!=[%08x]", adler1, adler2);
			}
			int64_t c = counts++;
			if (c % 1000 == 0)
			{
				CY_LOG(L_INFO, "Receive %lld", c);
			}

			std::set< Connection* >::iterator it;
			{
				sys_api::auto_mutex lock(this->clients_lock);
				for (it = this->clients.begin(); it != this->clients.end(); ++it) {
					(*it)->send(packet.get_buf(), packet.get_buf_length());
				}
			}
		}
	}

	//-------------------------------------------------------------------------------------
	virtual void on_close_callback(TcpServer*, int32_t thread_index, Connection* conn)
	{
		sys_api::auto_mutex lock(this->clients_lock);

		this->clients.erase(conn);
		CY_LOG(L_DEBUG, "connection %s:%d closed",
			conn->get_peer_addr().get_ip(),
			conn->get_peer_addr().get_port());
	}

	//-------------------------------------------------------------------------------------
	void on_extra_workthread_msg(TcpServer* server, int32_t thread_index, Packet* msg)
	{

	}
public:
	std::set< Connection* > clients;
	sys_api::mutex_t clients_lock;
	atomic_int64_t counts;
};

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{	
	uint16_t server_port = 8301;

	if (argc > 1)
		server_port = (uint16_t)atoi(argv[1]);

	CY_LOG(L_DEBUG, "listen port %d", server_port);

	ServerListener listener;
	listener.clients_lock = sys_api::mutex_create();
	listener.counts = 0;

	TcpServer* server = new TcpServer(&listener, "press", 0);
	server->bind(Address(server_port, false), true);

	if (!(server->start(sys_api::get_cpu_counts())))
		return 1;

	server->join();
	
	delete server;
	return 0;
}
