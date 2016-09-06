#include "cr_stdafx.h"
#include "cr_chat_package.h"

#include "GateMessage.pb.h"
#include "HOpCode.h"

#include <time.h>

//-------------------------------------------------------------------------------------
#define MIN_MESSAGE_LEN (8)
#define MAX_MESSAGE_LEN (256)

const char s_msg_character[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
const size_t s_msg_character_len = sizeof(s_msg_character)-1;

class ClientListener : public TcpClient::Listener
{
public:
	//-------------------------------------------------------------------------------------
	static bool _send_message_timer(Looper::event_id_t id, void* param)
	{
		ClientListener* listener = (ClientListener*)(param);

		if (listener->counts > 20) {
			listener->looper->disable_all(id);
			listener->looper->delete_event(id);

			CY_LOG(L_WARN, "DONE!");

			//listener->client->disconnect();
			return false;
		}
		char temp[1024] = { 0 };
#if 1
		//snprintf(temp, 1024, "Aa[%08d]", listener->counts);
		printf(">");
		scanf("%s", temp);
#else
		size_t len = rand() % (MAX_MESSAGE_LEN - MIN_MESSAGE_LEN) + MIN_MESSAGE_LEN;
		for (size_t i = 0; i < len; i++) {
			size_t index = rand() % s_msg_character_len;
			temp[i] = s_msg_character[index];
		}
#endif
	

		printf("---------\nsend %s\n", temp);

		ChatPackage package;
		package.buildChatMessage(0, temp);

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
			dhkey_t public_key;
			DH_generate_key_pair(public_key, private_key);

			//send hand shake
			CGHandshake handshake;
			handshake.set_version(4);
			handshake.set_dhkey_high(public_key.dq.high);
			handshake.set_dhkey_low(public_key.dq.low);
			
			ChatPackage package;
			package.buildFromProtobuf(::protobuf::message::CGHandshake, 0, &handshake);
			client->send(package.get_buf(), package.get_buf_length());

			return 0;
		}
		else
		{
			uint32_t retry_time = 1000 * 5;
			printf("connect failed!, retry after %d milli seconds...\n", retry_time);
			return 1000 * 5;
		}
	}

	//-------------------------------------------------------------------------------------
	void on_message_callback(TcpClient* /*client*/, Connection* conn)
	{
		RingBuf& buf = conn->get_input_buf();

		for (;;) {
			ChatPackage packet;
			if (!packet.buildFromRingBuf(buf)) return;

			switch (packet.get_packet_id()) {
			case ChatPackage::CHAT_MESSAGE_ID:
				{
					CY_LOG(L_DEBUG, "receive:%s", packet.get_content());
				}
				break;

			case ::protobuf::message::GCHandshake:
				{
					GCHandshake handshake;
					handshake.ParseFromArray(packet.get_content(), packet.get_packet_size());
					if (handshake.result() != 0) {
						CY_LOG(L_ERROR, "handshake error, result=%d", handshake.result());
						break;
					}

					dhkey_t other;
					other.dq.high = handshake.dhkey_high();
					other.dq.low = handshake.dhkey_low();
					DH_generate_key_secret(secret_key, private_key, other);

					char debug_out[128] = { 0 };
					for (int i = 0; i < DH_KEY_LENGTH; i++)
					{
						char temp[32] = { 0 };
						snprintf(temp, 32, "%02x", secret_key.bytes[i]);
						strcat(debug_out, temp);
					}
					CY_LOG(L_DEBUG, "agent=%d, seckey=%s", handshake.agent_id(), debug_out);

					this->looper->register_timer_event(1000, this, _send_message_timer);
				}
				break;
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
	uint32_t curr_server;

	dhkey_t private_key;
	dhkey_t secret_key;
};

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	const char* server_ip = "127.0.0.1";
	if (argc > 1)
		server_ip = argv[1];

	uint16_t server_port = 18201;
	if (argc > 2)
		server_port = (uint16_t)atoi(argv[2]);

	Address address(server_ip, server_port);
	
	time_t seed;
	time(&seed);
	srand((uint32_t)seed);

	ClientListener listener;
	listener.server_ip = server_ip;
	listener.port = server_port;
	listener.looper = Looper::create_looper();
	listener.client = new TcpClient(listener.looper, &listener, 0);
	listener.counts = 0;
	listener.curr_server = 1;

	listener.client->connect(address, 1000 * 3);
	listener.looper->loop();

	return 0;
}

