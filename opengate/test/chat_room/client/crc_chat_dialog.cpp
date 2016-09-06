#include "cr_stdafx.h"
#include "crc_chat_dialog.h"
#include "cr_chat_package.h"

#include "GateMessage.pb.h"
#include "ChatMessage.pb.h"
#include "HOpCode.h"


uint32_t CChatDialog::ClientListener::on_connection_callback(TcpClient* client, bool success)
{
	if (success)
	{
		srand(0);

		dhkey_t public_key;
		DH_generate_key_pair(public_key, private_key);

		//send hand shake
		CGHandshake handshake;
		handshake.set_version(4);
		handshake.set_dhkey_high(public_key.dq.high);
		handshake.set_dhkey_low(public_key.dq.low);

		ChatPackage package;
		package.buildFromProtobuf(::protobuf::message::CGHandshake, 0, &handshake, 0);
		client->send(package.get_buf(), package.get_buf_length());

		::SendMessage(wnd, CChatDialog::WM_CHAT_CONNECTED, 0, 0);

		return 0;
	}
	else
	{
		uint32_t retry_time = 1000 * 5;
		printf("connect failed!, retry after %d milli seconds...\n", retry_time);
		return 1000 * 5;
	}

}

void CChatDialog::ClientListener::on_message_callback(TcpClient* client, Connection* conn)
{
	RingBuf& buf = conn->get_input_buf();

	for (;;) {
		ChatPackage packet;
		if (!packet.buildFromRingBuf(buf, (need_encrypt? &m_decrypt :0))) return;

		switch (packet.get_packet_id()) {
		case ::protobuf::message::CGChat:
		{
			CGChat chatMessage;
			chatMessage.ParseFromArray(packet.get_content(), packet.get_packet_size());

			::SendMessage(wnd, CChatDialog::WM_CHAT_MESSAGE_RECEIVE, (WPARAM)chatMessage.content().c_str(), chatMessage.agentid());
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
			if (other.dq.high != 0 && other.dq.low != 0) {
				need_encrypt = true;
				DH_generate_key_secret(secret_key, private_key, other);
				m_encrypt.seed0 = secret_key.dq.low;
				m_encrypt.seed1 = secret_key.dq.high;

				m_decrypt.seed0 = secret_key.dq.high;
				m_decrypt.seed1 = secret_key.dq.low;
			}

			char debug_out[128] = { 0 };
			for (int i = 0; i < DH_KEY_LENGTH; i++)
			{
				char temp[32] = { 0 };
				snprintf(temp, 32, "%02x", secret_key.bytes[i]);
				strcat(debug_out, temp);
			}
			CY_LOG(L_DEBUG, "agent=%d, seckey=%s", handshake.agent_id(), debug_out);
			::SendMessage(wnd, CChatDialog::WM_CHAT_HANDSHAKE, (WPARAM)handshake.agent_id(), (LPARAM)debug_out);
		}
		break;
		}

	}
}

void CChatDialog::ClientListener::on_close_callback(TcpClient* client)
{

}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////
CChatDialog::CChatDialog()
{
}

//--------------------------------------------------------------------------------------------
CChatDialog::~CChatDialog()
{
}

//--------------------------------------------------------------------------------------------
void CChatDialog::_chat_thread(void* param)
{
	CChatDialog* pThis = (CChatDialog*)param;
	CChatDialog::ClientListener& listener = pThis->listener;

	const char* server_ip = "127.0.0.1";
	uint16_t server_port = 18201;

	Address address(server_ip, server_port);

	listener.wnd = pThis->m_hWnd;
	listener.server_ip = server_ip;
	listener.port = server_port;
	listener.looper = Looper::create_looper();
	listener.client = new TcpClient(listener.looper, &listener, 0);
	listener.counts = 0;
	listener.curr_server = 1;
	listener.need_encrypt = false;

	listener.client->connect(address, 1000 * 3);
	listener.looper->loop();
}

//--------------------------------------------------------------------------------------------
LRESULT CChatDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	sys_api::thread_create(_chat_thread, this, "chat_thread");


	::SetFocus(::GetWindow(m_hWnd, IDC_EDIT_CHAT));
	return (LRESULT)TRUE;
}

//--------------------------------------------------------------------------------------------
LRESULT CChatDialog::OnChatMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	switch (uMsg)
	{
	case WM_CHAT_CONNECTED:
		ListBox_AddString(GetDlgItem(IDC_LIST_CHAT), "Connected!");
		break;

	case WM_CHAT_HANDSHAKE:
	{
		char debug_msg[256] = { 0 };
		_snprintf(debug_msg, 256, "agentid=%d, key=%s", (int)wParam, (const char*)lParam);
		agentID = (uint32_t)wParam;
		ListBox_AddString(GetDlgItem(IDC_LIST_CHAT), debug_msg);

		_snprintf(debug_msg, 256, "Agent%d", (int)wParam);
		SetWindowTextA(debug_msg);

	}
		break;

	case WM_CHAT_MESSAGE_RECEIVE:
	{
		const char* message = (const char*)wParam;
		uint32_t agentid = (uint32_t)lParam;
		char temp[1024] = { 0 };
		_snprintf(temp, 1024, "Agent%d:%s", agentid, message);

		ListBox_AddString(GetDlgItem(IDC_LIST_CHAT), temp);
	}
		break;

	default:
		break;
	}

	return (LRESULT)TRUE;
}

//--------------------------------------------------------------------------------------------
LRESULT CChatDialog::OnOKCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	return 0;
}

//--------------------------------------------------------------------------------------------
LRESULT CChatDialog::OnCancelCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{

	EndDialog(0);
	return 0;
}

//--------------------------------------------------------------------------------------------
LRESULT CChatDialog::OnSendButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char szTemp[1024] = { 0 };

	this->GetDlgItemTextA(IDC_EDIT_CHAT, szTemp, 1024);

	CGChat chatMsg;
	chatMsg.set_content(szTemp);
	chatMsg.set_agentid(agentID);

	ChatPackage package;
	package.buildFromProtobuf(::protobuf::message::CGChat, 0, &chatMsg, listener.need_encrypt ? &listener.m_encrypt : 0);

	listener.client->send(package.get_buf(), package.get_buf_length());

	this->SetDlgItemTextA(IDC_EDIT_CHAT, "");
	::SetFocus(::GetDlgItem(m_hWnd, IDC_EDIT_CHAT));

	listener.counts += 1;

	return TRUE;
}