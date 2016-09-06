#include "og_stdafx.h"
#include "og_down_service.h"
#include "og_message_define.h"
#include "og_agent_service.h"
#include "og_config.h"
#include "og_up_service.h"

#include "GateMessage.pb.h"
#include "HOpCode.h"

//-------------------------------------------------------------------------------------
bool DownService::_process_gate_message(const Packet& gate_message, int32_t thread_index, Connection* conn)
{
	switch (gate_message.get_packet_id())
	{
	case protobuf::message::CGHandshake:
		{
			ConnectionProxy* proxy = (ConnectionProxy*)(conn->get_proxy());
			if (proxy->state != ConnectionProxy::kCreated) 
			{
				CY_LOG(L_WARN, "Agent_%d Receive CGHandshake msg when agent is created, connection id=%d[%p], status=%d", 
					proxy->agent_id, conn->get_id(), conn, proxy->state);
				return false;
			}

			CGHandshake handshake;
			if (!handshake.ParseFromArray(gate_message.get_packet_content(), gate_message.get_packet_size()))
			{
				CY_LOG(L_ERROR, "Parse CGHandshake msg failed, connection id=%d[%p]", 
					conn->get_id(), conn);
				return false;
			}

			//check version
			if (handshake.version() != m_config->get_down_stream().handshake_ver) 
			{
				CY_LOG(L_ERROR, "Handshake version error, connection id=%d[%p], remoteip=%s, ver=%d, need=%d", 
					conn->get_id(), conn, conn->get_peer_addr().get_ip(), 
					handshake.version(), m_config->get_down_stream().handshake_ver);

				GCHandshake handshake_reply;
				handshake_reply.set_result(1);	//version error!

				Packet reply_msg;
				reply_msg.build(GAME_MESSAGE_HEAD_SIZE, protobuf::message::GCHandshake, (uint16_t)handshake_reply.ByteSize(), 0);
				handshake_reply.SerializeToArray(reply_msg.get_memory_buf() + GAME_MESSAGE_HEAD_SIZE, reply_msg.get_packet_size());
				conn->send(reply_msg.get_memory_buf(), reply_msg.get_memory_size());

				return false;
			}

			//check up connection status
			if (!UpService::getSingleton()->is_up_connection_stable()) 
			{
				CY_LOG(L_ERROR, "Receive CGHandshake when up stream not ready, connection id=%d[%p], remoteip=%s", 
					conn->get_id(), conn, conn->get_peer_addr().get_ip());

				GCHandshake handshake_reply;
				handshake_reply.set_result(2);	//up stream not ready!

				Packet reply_msg;
				reply_msg.build(GAME_MESSAGE_HEAD_SIZE, protobuf::message::GCHandshake, (uint16_t)handshake_reply.ByteSize(), 0);
				handshake_reply.SerializeToArray(reply_msg.get_memory_buf() + GAME_MESSAGE_HEAD_SIZE, reply_msg.get_packet_size());
				conn->send(reply_msg.get_memory_buf(), reply_msg.get_memory_size());

				return false;
			}

			//update state
			proxy->state = ConnectionProxy::kHandShaking;

			//request new agent
			{
				RequestNewAgent request;
				request.down_thread_index = thread_index;
				request.agent_thread_index = proxy->agent_thread_index;
				request.agent_id = proxy->agent_id;
				request.connection_id = conn->get_id();
				request.public_key.dq.low = handshake.dhkey_low();
				request.public_key.dq.high = handshake.dhkey_high();

				Packet msg;
				msg.build(WorkThread::MESSAGE_HEAD_SIZE, RequestNewAgent::ID, sizeof(request), (const char*)&request);
				AgentService::getSingleton()->send_message(proxy->agent_thread_index, &msg);
			}
		
			CY_LOG(L_DEBUG, "Receive CGHandshake msg, ver=%d, remote=%s", 
				handshake.version(), conn->get_peer_addr().get_ip());
		}
		break;

	case protobuf::message::GateHeartbeat:
		{
			ConnectionProxy* proxy = (ConnectionProxy*)(conn->get_proxy());
			GateHeartbeat msg;
			msg.ParseFromArray(gate_message.get_packet_content(), gate_message.get_packet_size());

			CY_LOG(L_DEBUG, "Agent_%d receive HEARTBEAT msg, time=%lld", proxy->agent_id, msg.time());

			GateHeartbeat reply;
			reply.set_time(::time(0));

			Packet reply_msg;
			reply_msg.build(GAME_MESSAGE_HEAD_SIZE, protobuf::message::GateHeartbeat, (uint16_t)reply.ByteSize(), 0);
			reply.SerializeToArray(reply_msg.get_memory_buf() + GAME_MESSAGE_HEAD_SIZE, reply_msg.get_packet_size());
			conn->send(reply_msg.get_memory_buf(), reply_msg.get_memory_size());
		}
		break;
	}
	return true;
}