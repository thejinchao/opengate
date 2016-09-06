#include "og_stdafx.h"
#include "og_debug.h"
#include "og_down_service.h"
#include "og_agent_service.h"
#include "og_up_service.h"

#ifndef _WIN32
#include <hiredis/hiredis.h>
#endif

//-------------------------------------------------------------------------------------
Debug* Debug::s_singleton = 0;

//-------------------------------------------------------------------------------------
Debug::Debug()
	: m_enable(false)
	, m_thread_counts(0)
	, m_redis_threads(0)
{
	s_singleton = this;
}

//-------------------------------------------------------------------------------------
Debug::~Debug()
{
	//TODO: close redis connection
}

//-------------------------------------------------------------------------------------
bool Debug::init_redis_thread(const char* redis_host, uint16_t redis_port, int32_t thread_counts)
{
#ifdef _WIN32
	return false;
#else
	assert(m_thread_counts == 0);
	assert(thread_counts > 0 && redis_host && redis_host[0] && redis_port>0);

	m_thread_counts = thread_counts;
	m_redis_host = redis_host;
	m_redis_port = redis_port;

	//create work thread pool
	m_redis_threads = new RedisThread[thread_counts];
	for (int32_t i = 0; i < thread_counts; i++)
	{
		char name[128] = { 0 };
		snprintf(name, 128, "dbg_%d", i);

		m_redis_threads[i].debug = this;
		m_redis_threads[i].index = i;
		m_redis_threads[i].contex = 0;
		m_redis_threads[i].ready = 0;
		m_redis_threads[i].msg_signal = sys_api::signal_create();
		m_redis_threads[i].thread = sys_api::thread_create(_redis_thread_func_entry, &(m_redis_threads[i]), name);

		//wait...
		while (m_redis_threads[i].ready == 0) sys_api::thread_yield();

		//check result
		if (m_redis_threads[i].ready > 1) {
			CY_LOG(L_ERROR, "Connect to redis failed!");
			return false;
		}
	}

	m_enable = true;
	return true;
#endif
}

//-------------------------------------------------------------------------------------
bool Debug::init_monitor_thread(int32_t debug_fraq)
{
#ifdef _WIN32
	return false;
#else
	if (!m_enable) return false;

	m_debug_fraq = debug_fraq;
	//create monitor thread
	m_monitor_thread = sys_api::thread_create(_monitor_thread_func_entry, this, "monitor");

	return true;
#endif
}

//-------------------------------------------------------------------------------------
redisContext* Debug::_connect_to_redis(bool flushdb)
{
#ifdef _WIN32
	return 0;
#else
	//begin connect to redis
	struct timeval timeout = { 1, 500000 }; // 1.5 seconds
	redisContext* contex = redisConnectWithTimeout(m_redis_host.c_str(), m_redis_port, timeout);
	if (contex == 0 || contex->err) {
		return 0;
	}

	//select db
	redisReply* reply = (redisReply*)redisCommand(contex, "SELECT 9");
	freeReplyObject(reply);

	//clean current db
	if(flushdb) {
		reply = (redisReply*)redisCommand(contex, "FLUSHDB");
		freeReplyObject(reply);
	}

	return contex;
#endif
}

//-------------------------------------------------------------------------------------
void Debug::_redis_thread_func(RedisThread* param)
{
#ifdef _WIN32
	return;
#else
	CY_LOG(L_DEBUG, "debug[%d] thread start, connect redis %s:%d", param->index, m_redis_host.c_str(), m_redis_port);

	param->contex = _connect_to_redis(param->index==0);
	if(param->contex==0) {
		param->ready = 2;
		CY_LOG(L_DEBUG, "redis connect failed!");
		return;
	}
	
	CY_LOG(L_DEBUG, "redis connect ok!");
	param->ready = 1;

	//begin work
	for (;;)
	{
		//wait cmd
		sys_api::signal_wait(param->msg_signal);

		for (;;) {
			std::string* cmd;
			if (!(param->ms_queue.pop(cmd))) break;

			//process the packet
			redisReply* reply = (redisReply*)redisCommand(param->contex, cmd->c_str());
			freeReplyObject(reply);

			//delete the packet
			delete cmd;
		}
	}
#endif
}

//-------------------------------------------------------------------------------------
void Debug::set_debug_value(const char* key, int32_t value)
{
#ifdef _WIN32
	return;
#else
	if (!m_enable) return;

	char str_value[32];
	snprintf(str_value, 32, "%d", value);

	set_debug_value(key, str_value);
#endif
}

//-------------------------------------------------------------------------------------
void Debug::set_debug_value(const char* key, const char* value)
{
#ifdef _WIN32
	return;
#else
	if(!m_enable) return;

	char temp[2048] = { 0 };
	snprintf(temp, 2048, "SET %s %s", key, value);

	uint32_t adler = adler32(0, 0, 0);
	int32_t index = adler32(adler, key, strlen(key)) % m_thread_counts;
	RedisThread& thread = m_redis_threads[index];

	std::string* cmd = new std::string(temp);
	thread.ms_queue.push(cmd);
	sys_api::signal_notify(thread.msg_signal);
#endif

}

//-------------------------------------------------------------------------------------
void Debug::del_debug_value(const char* key)
{
#ifdef _WIN32
	return;
#else
	if (!m_enable) return;

	uint32_t adler = adler32(0, 0, 0);
	int32_t index = adler32(adler, key, strlen(key)) % m_thread_counts;
	RedisThread& thread = m_redis_threads[index];

	char temp[MAX_PATH] = { 0 };
	snprintf(temp, MAX_PATH, "DEL %s", key);

	std::string* cmd = new std::string(temp);
	thread.ms_queue.push(cmd);
	sys_api::signal_notify(thread.msg_signal);
#endif
}

//-------------------------------------------------------------------------------------
void Debug::_monitor_thread_func(void)
{
#ifdef _WIN32
	return;
#else
	CY_LOG(L_DEBUG, "Monitor thread function start, debug per %d second(s)", m_debug_fraq);

	for (;;) {
		DownService::getSingleton()->debug();
		AgentService::getSingleton()->debug();
		UpService::getSingleton()->debug();

		//AgentSer
		sys_api::thread_sleep(m_debug_fraq * 1000);
	}
#endif
}
