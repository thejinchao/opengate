#pragma once

//pre-define
class AgentThread;
class Config;

class AgentService
{
public:
	/// start the agent thread pool(NOT thread safe)
	bool start(int32_t thread_counts, Config* config);

	/// send message to one thread, if thread_index<0, pick next thred(thread safe)
	void send_message(int32_t thread_index, const Packet* message);

	/// get next agent id(thread safe)
	int32_t get_next_agent_id(void);

	/// get packet desc(thread safe)
	const char* get_packet_desc(int32_t id) const;

	/// get thread counts
	int32_t get_thread_counts(void) const { return m_thread_counts; }
	int32_t get_next_thread(void);

	/// debug
	void debug(void);

private:
	enum { MAX_THREAD_COUNTS = 64 };
	int32_t			m_thread_counts;
	AgentThread*	m_thread_pool[MAX_THREAD_COUNTS];

#ifdef CY_SYS_WINDOWS
	typedef std::hash_map< int32_t, const char* > PacketDescMap;
#else
	typedef __gnu_cxx::hash_map< int32_t, const char* > PacketDescMap;
#endif
	PacketDescMap	m_packet_desc_map;

private:
	atomic_int32_t m_next_thread_id;
	atomic_int32_t m_next_agent_id;
	Config* m_config;

public:
	AgentService();
	~AgentService();

	static AgentService* getSingleton(void) { return s_singleton; }
private:
	static AgentService* s_singleton;
};
