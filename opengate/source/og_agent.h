#pragma once

class Config;

class Agent
{
public:
	int32_t get_down_thread_index(void) const { return m_down_thread_index; }
	int32_t get_connection_id(void) const { return m_connection_id; }

	int32_t get_agent_thread_index(void) const { return m_agent_thread_index; }
	int32_t get_agent_id(void) const { return m_agent_id; }

	int32_t get_up_thread_index(void) const { return m_up_thread_index; }

	void on_up_server_connected(void)
	{
		m_up_valid = true;
	}

	void reset_up_server(void)
	{
		m_up_valid = false;
	}

	bool is_up_server_valid(void) const { return m_up_valid; }

	void key_exchange(const dhkey_t& another_pub_key, dhkey_t& public_key);

	const dhkey_t& get_secret_key(void) const { return m_secret_key; }
	void get_public_key(dhkey_t& public_key) const { 
		public_key.dq.low = m_public_key.dq.low; 
		public_key.dq.high = m_public_key.dq.high; 
	}

	void log_packet(Packet* packet);

	int32_t get_total_down_size(void) const { return m_total_down_size; }
	int32_t get_total_up_size(void) const { return m_total_up_size; }

	void decrypt_packet(uint8_t* message, size_t message_size);
	void encrypt_packet(uint8_t* message, size_t message_size);

	void debug(DebugInterface* debuger);
	void del_debug_value(DebugInterface* debuger);

private:
	int32_t m_down_thread_index;
	int32_t m_connection_id;

	int32_t m_agent_thread_index;
	int32_t	m_agent_id;

	bool m_up_valid;
	int32_t m_up_thread_index;

	dhkey_t m_public_key;
	dhkey_t m_secret_key;

	std::string m_packet_log_file;

	int32_t m_total_down_size;
	int32_t m_total_up_size;

	XorShift128	m_encrypt;
	XorShift128 m_decrypt;

	Config* m_config;

public:
	Agent(int32_t down_thread_index, int32_t connection_id, 
		int32_t agent_thread_index, int32_t agent_id, 
		int32_t up_thread_index, 
		const char* packet_log_file, 
		Config* config);
	~Agent();
};
