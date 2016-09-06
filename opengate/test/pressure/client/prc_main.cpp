#include "pr_stdafx.h"
#include "pr_package.h"

#include "GateMessage.pb.h"

#include <time.h>

//-------------------------------------------------------------------------------------
#define MIN_MESSAGE_LEN (16)
#define MAX_MESSAGE_LEN (1000)

const char s_msg_character[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
const size_t s_msg_character_len = sizeof(s_msg_character)-1;

struct client_s
{
	std::string gate_ip;
	int gate_port;

	int counts_per_thread;
};

class ClientListener : public TcpClient::Listener
{
public:
	//-------------------------------------------------------------------------------------
	static bool _send_message_timer(Looper::event_id_t id, void* param)
	{
		ClientListener* listener = (ClientListener*)(param);

		char temp[1024] = { 0 };
		float r = (float)rand() / RAND_MAX;
		size_t len = (size_t)((r*r*r) * (MAX_MESSAGE_LEN - MIN_MESSAGE_LEN)) + MIN_MESSAGE_LEN;
		for (size_t i = 0; i < len; i++) {
			size_t index = rand() % s_msg_character_len;
			temp[i] = s_msg_character[index];
		}

		uint32_t adler = adler32(0, 0, 0);
		adler = adler32(adler, temp, len);
		memcpy(temp + len, &adler, sizeof(adler));

		//CY_LOG(L_DEBUG, "---------%ld", sys_api::time_now());

		Package package;
		package.buildFromMessage(temp, len+4);

		listener->client->send(package.get_buf(), package.get_buf_length());

		listener->counts += 1;

		return false;
	}

	//-------------------------------------------------------------------------------------
	uint32_t on_connection_callback(TcpClient* client, bool success)
	{
		CY_LOG(L_DEBUG, "connect to %s:%d %s.",
			client->get_server_address().get_ip(),
			client->get_server_address().get_port(),
			success ? "OK" : "FAILED");

		if (success)
		{
			//send handshake
			CGHandshake msg;
			msg.set_version(3);
			msg.set_dhkey_high(0);
			msg.set_dhkey_low(0);

			Package package;
			package.buildFromProtobuf(102, &msg);

			client->send(package.get_buf(), package.get_buf_length());
		}

		return 0;
	}

	//-------------------------------------------------------------------------------------
	void on_message_callback(TcpClient* /*client*/, Connection* conn)
	{
		RingBuf& buf = conn->get_input_buf();

		for (;;) {
			Package packet;
			if (!packet.buildFromBuf(buf)) return;

			if (packet.get_Packet_id() == 102) {
				CY_LOG(L_DEBUG, "receive:HANDSHAKE");
				this->looper->register_timer_event(1000, this, _send_message_timer);
			}
			else {
				//CY_LOG(L_DEBUG, "receive:%s", packet.get_content());
			}

		}
	}

	//-------------------------------------------------------------------------------------
	void on_close_callback(TcpClient* client)
	{
		(void)client;

		CY_LOG(L_DEBUG, "socket close");
		exit(0);
	}

public:
	const char* server_ip;
	uint16_t port;
	TcpClient* client;
	Looper* looper;
	uint32_t counts;
};

//-------------------------------------------------------------------------------------
void _thread_function(void* param)
{
	client_s* client = (client_s*)param;

	Address address(client->gate_ip.c_str(), (uint16_t)(client->gate_port));

	uint32_t seed = (uint32_t)time(0) + (uint32_t)sys_api::thread_get_current_id();
	srand(seed);

	Looper* looper = Looper::create_looper();

	ClientListener* listener = new ClientListener[client->counts_per_thread];

	for (int i = 0; i < client->counts_per_thread; i++)
	{
		listener[i].server_ip = client->gate_ip.c_str();
		listener[i].port = client->gate_port;
		listener[i].looper = looper;
		listener[i].client = new TcpClient(looper, &(listener[i]), 0);
		listener[i].counts = 0;

		listener[i].client->connect(address, 1000 * 3);
	}


	looper->loop();
}

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	const char* server_ip = "127.0.0.1";
	if (argc > 1)
		server_ip = argv[1];

	uint16_t server_port = 18201;
	if (argc > 2)
		server_port = (uint16_t)atoi(argv[2]);

	CY_LOG(L_INFO, "press test client start");

	client_s client;
	client.gate_ip = server_ip;
	client.gate_port = server_port;
	client.counts_per_thread = 20;

	int thread_counts = sys_api::get_cpu_counts();
	thread_t* t = new thread_t[thread_counts];

	for (int i = 0; i < thread_counts; i++) {
		char name[32] = { 0 };
		snprintf(name, 32, "thread%d", i);
		t[i] = sys_api::thread_create(_thread_function, &client, name);
	}

	sys_api::thread_join(t[0]);
	
	return 0;
}

