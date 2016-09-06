#pragma once

#include <string>

/*

#config format
[global]
enable_packet_encrypt = 1
enable_packet_log = 1			;enable log all packet

enable_debug = 1
debug_fraq = 5					;write debug value per x second(s)
redis_host = 127.0.0.1
redis_port = 6379

[down_stream]
server_ip=0.0.0.0				;listen address
server_port=18201               ;listen port
handshake_ver = 1

[up_stream]
server_ip=127.0.0.1
server_port=8201
thread_counts=0;                ;0: means cpu counts

*/

class Config
{
public:
	struct UpStream
	{
		std::string server_ip;
		uint32_t	server_port;
		uint32_t	thread_counts;
	};

	struct DownStream
	{
		std::string server_ip;
		uint32_t	server_port;
		uint32_t	thread_counts;
		uint32_t	handshake_ver;
	};

public:
	bool load(const char* config_file);
	void clear();

public:
	const DownStream& get_down_stream(void) const { return m_down_stream; }
	const UpStream& get_up_stream(void) const { return m_up_stream; }
	bool is_packet_encrypt_enable(void) const { return m_enable_packet_encrypt; }
	bool is_packet_log_enable(void) const { return m_enable_packet_log; }
	bool is_debug_enable(void) const { return m_enable_debug; }
	const std::string get_redis_host(void) const { return m_redis_host; }
	int32_t get_redis_port(void) const { return m_redis_port; }
	int32_t get_debug_fraq(void) const { return m_debug_fraq; }

private:
	bool		m_enable_packet_encrypt;
	bool		m_enable_packet_log;
	bool		m_enable_debug;
	std::string	m_redis_host;
	int32_t		m_redis_port;
	int32_t		m_debug_fraq;

	DownStream	m_down_stream;
	UpStream	m_up_stream;

private:
	void _default(void);
	uint32_t _cpu_counts(void);

	int32_t		_get_as_int(const char* value);
	std::string _get_as_string(const char* value);

	//for ini parser
	static int _config_handler_entry(void* user, const char* section, const char* name, const char* value){
		return ((Config*)user)->_config_handler(section, name, value);
	}
	bool _config_handler(const char* section, const char* name, const char* value);

	bool _push_current_up_stream(void);

	bool m_down_stream_loaded;
	bool m_up_stream_loaded;

	uint32_t m_cpu_counts;

public:
	Config();
	~Config();
};