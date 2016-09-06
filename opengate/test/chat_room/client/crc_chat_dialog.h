#pragma once

#include "resource.h"

class CChatDialog : public CDialogImpl< CChatDialog >
{
public:
	enum { IDD = IDD_DIALOG_CHAT };
	enum {
		WM_CHAT_CONNECTED=WM_USER+100,
		WM_CHAT_HANDSHAKE,
		WM_CHAT_MESSAGE_RECEIVE,
		
	};

	BEGIN_MSG_MAP(CChatDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_RANGE_HANDLER(WM_CHAT_CONNECTED, WM_CHAT_MESSAGE_RECEIVE, OnChatMessage)
		COMMAND_ID_HANDLER(IDOK, OnOKCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancelCmd)
		COMMAND_ID_HANDLER(ID_BUTTON_SEND, OnSendButton)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnChatMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnOKCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnCancelCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnSendButton(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

private:
	class ClientListener : public TcpClient::Listener
	{
	public:
		virtual uint32_t on_connection_callback(TcpClient* client, bool success);
		virtual void on_message_callback(TcpClient* client, Connection* conn);
		virtual void on_close_callback(TcpClient* client);
	public:
		HWND wnd;
		const char* server_ip;
		uint16_t port;
		TcpClient* client;
		Looper* looper;
		uint32_t counts;
		uint32_t curr_server;

		dhkey_t private_key;
		dhkey_t secret_key;
		bool need_encrypt;

		XorShift128	m_encrypt;
		XorShift128 m_decrypt;
	};

	ClientListener listener;
	uint32_t agentID;

	static void _chat_thread(void* param);

public:
	CChatDialog();
	~CChatDialog();
};
