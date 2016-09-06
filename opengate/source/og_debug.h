#pragma once

struct redisContext;

/*
*HOW TO* print all key and value of debug db(no.9 db)?

echo -e "SELECT 9\nKEYS *" | redis-cli | awk '{if(NR==1){print "SELECT 9"} else { print "echo " $0 " \n GET " $0}}' | redis-cli | tail -n +2 | awk '{a=$1;getline;print a "\t" $1;next}'

*/
class Debug : public cyclone::DebugInterface
{
public:

	bool init_redis_thread(const char* redis_host, uint16_t redis_port, int32_t thread_counts);
	bool init_monitor_thread(int32_t debug_fraq);

	virtual bool is_enable(void) { return m_enable; }
	virtual void set_debug_value(const char* key, const char* value);
	virtual void set_debug_value(const char* key, int32_t value);
	virtual void del_debug_value(const char* key);

private:
	bool		m_enable;
	int32_t		m_thread_counts;
	std::string m_redis_host;
	int16_t		m_redis_port;
	int32_t		m_debug_fraq;

	typedef LockFreeQueue<std::string*> CmdQueue;

	struct RedisThread
	{
		Debug*			debug;
		int32_t			index;
		thread_t		thread;
		redisContext*	contex;
		atomic_int32_t	ready;

		CmdQueue			ms_queue;
		sys_api::signal_t	msg_signal;

	};
	RedisThread*	m_redis_threads;

	thread_t		m_monitor_thread;

private:
	static void _redis_thread_func_entry(void* param) {
		((RedisThread*)param)->debug->_redis_thread_func((RedisThread*)param);
	}
	void _redis_thread_func(RedisThread* param);

	static void _monitor_thread_func_entry(void* param) {
		((Debug*)param)->_monitor_thread_func();
	}
	void _monitor_thread_func(void);

	redisContext* _connect_to_redis(bool flushdb);

public:
	Debug();
	~Debug();

	static Debug* getSingleton(void) { return s_singleton; }
private:
	static Debug* s_singleton;
};
