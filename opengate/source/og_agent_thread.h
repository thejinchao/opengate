#pragma once

//pre-define
class Agent;
class Config;

class AgentThread
{
public:
	//// send message to this work thread (thread safe)
	void send_message(const Packet* message);

	//// get work thread index in work thread pool (thread safe)
	int32_t get_index(void) const { return m_thread_index; }

	//// is current thread in work thread (thread safe)
	bool is_in_workthread(void) const;

private:
	int32_t			m_thread_index;
	thread_t		m_thread;

	typedef LockFreeQueue<Packet*> MessageQueue;
	MessageQueue		m_message_queue;
	sys_api::signal_t	m_message_signal;

#ifdef CY_SYS_WINDOWS
	typedef std::hash_map< int32_t, Agent* > AgentMap;
#else
	typedef __gnu_cxx::hash_map< int32_t, Agent* > AgentMap;
#endif
	AgentMap		m_agent_map;
	std::string		m_packet_log_dir;
	Config*			m_config;

	enum { MAX_FORWORD_DOWN_MSG_COUNTS = 32 };
	size_t m_down_stream_counts;
	typedef std::vector<Packet*> MessageVector;
	MessageVector*	m_forword_down_batch;

private:
	static void _work_thread_entry(void* param) {
		((AgentThread*)param)->work_thread_func();
	}
	void work_thread_func(void);

private:
	void _on_message(Packet* message);

	void _create_agent(Packet* message);
	void _on_up_connect_result(Packet* message);
	void _forword_message_up(Packet* message);
	void _forword_message_down(Packet* message);
	void _forword_message_down_end(void);
	void _delete_agent_up(Packet* message);
	void _delete_agent_down(Packet* message);
	void _delete_all_agent(void);
	void _debug(Packet* message);

public:
	AgentThread(int32_t index, const char* name, const char* packet_log_dir, Config* config);
	~AgentThread();
};
