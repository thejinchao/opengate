#pragma once

#include "og_config.h"

class UpThread 
	: public WorkThread::Listener
	, public TcpClient::Listener
{
public:
	//// send message to this work thread (thread safe)
	void send_message(Packet* message);

	//// get work thread index in work thread pool (thread safe)
	int32_t get_index(void) const { return m_thread_index; }

	//// is current thread in work thread (thread safe)
	bool is_in_workthread(void) const;

private:
	const Config::UpStream* m_config;

	int32_t		m_thread_index;
	WorkThread*	m_work_thread;
	TcpClient*	m_client;

	enum ClientState { kCreated, kReady, kClosing };
	struct ClientProxy
	{
		int32_t		agent_thread_index;
		int32_t		agent_id;

		ClientState	state;
	};

#ifdef CY_SYS_WINDOWS
	typedef std::hash_map< int32_t, ClientProxy* > TcpClientMap;
#else
	typedef __gnu_cxx::hash_map< int32_t, ClientProxy* > TcpClientMap;
#endif
	TcpClientMap	m_clientMap;

	int64_t			m_total_up_size;
	int64_t			m_total_down_size;

private:
	void _create_new_agent(Packet* message);
	void _forword_message(Packet* message);
	void _delete_agent(Packet* message);
	void _restart_connect(Packet* message);
	void _debug(Packet* message);

	// gate message send from server to up thread
	void _process_gate_message(const Packet* message);

private:
	//// called by message port (in work thread)
	virtual bool on_workthread_start(void);
	virtual bool on_workthread_message(Packet*);

	/// called by TcpClient(in work thread)
	virtual uint32_t on_connection_callback(TcpClient* client, bool success);
	virtual void on_message_callback(TcpClient* client, Connection* conn);
	virtual void on_close_callback(TcpClient* client);

public:
	UpThread(const Config::UpStream* config, int32_t index, const char* name);
	~UpThread();
};