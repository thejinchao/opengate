#pragma once

//pre-define
class Config;

class DownService : public TcpServer::Listener
{
public:
	/// start the down server
	bool start(const Config* config, DebugInterface* debuger);
	//wait for loop quit
	void join(void);
	/// send message to one thread, if thread_index<0, pick next thred(thread safe)
	void send_message(int32_t thread_index, const Packet* message);
	void send_message(int32_t thread_index, const Packet** message, int32_t counts);
	/// get thread counts
	int32_t get_thread_counts(void) const;
	//terminate the looper(called by signal handler)
	static void signal_terminate(void)	{
		getSingleton()->_signal_terminate();
	}
	void debug(void);

private:
	virtual void on_connection_callback(TcpServer* server, int32_t thread_index, Connection* conn);
	virtual void on_message_callback(TcpServer* server, int32_t thread_index, Connection* conn);
	virtual void on_close_callback(TcpServer* server, int32_t thread_index, Connection* conn);
	virtual void on_extra_workthread_msg(TcpServer* server, int32_t thread_index, Packet* msg);

private:
	TcpServer*		m_server;
	const Config*	m_config;

	struct ConnectionProxy
	{
		Connection* conn;

		enum AgentState{ kCreated=0, kHandShaking, kHandShaked, kClosing };
		AgentState	state;
		int32_t		agent_thread_index;
		int32_t		agent_id;
	};

#ifdef CY_SYS_WINDOWS
	typedef std::hash_map< int32_t, ConnectionProxy* > ConnectionProxyMap;
#else
	typedef __gnu_cxx::hash_map< int32_t, ConnectionProxy* > ConnectionProxyMap;
#endif

	struct ServerThread
	{
		int32_t thread_index;
		ConnectionProxyMap connectionMap;
	};
	int32_t m_work_thread_counts;
	ServerThread* m_work_threads;

private:
	void _on_replay_new_agent(int32_t thread_index, Packet* packet);
	void _forword_message_down(int32_t thread_index, Packet* packet);
	void _on_del_agent(int32_t thread_index, Packet* packet);
	void _delete_all_agent(int32_t thread_index);

	bool _process_gate_message(const Packet& packet, int32_t thread_index, Connection* conn);

	void _signal_terminate(void);

public:
	DownService();
	~DownService();

	static DownService* getSingleton(void) { return s_singleton; }
private:
	static DownService* s_singleton;
};
