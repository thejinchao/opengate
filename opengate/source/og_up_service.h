#pragma once

//pre-define
class Config;
class UpThread;

class UpService
{
public:
	//NOT thread safe
	bool start(const Config* config);

	//pick the next up thread, if thread_index<0, pick next thread in group (thread safe)
	UpThread* pick_stream(int32_t thread_index) const;

	/// is all up connection has been connected(thread safe)
	bool is_up_connection_stable(void) const { return m_up_connection_counts == (int32_t)m_thread_counts; }

	/// on up connection connected or disconnected(thread safe)
	void on_up_connection(bool conn, int32_t handshake_version);
	/// get current handshake version
	int32_t get_handshake_version(void) const { return m_handshake_version; }

	/// get next up thread
	int32_t get_next_thread(void) { return (m_next_thread++) % (int32_t)m_thread_counts; }

	/// debug
	void debug(void);

private:
	UpThread**		m_threads;
	size_t			m_thread_counts;
	mutable atomic_int32_t	m_next_thread;

	mutable atomic_int32_t	m_up_connection_counts;
	mutable atomic_int32_t m_handshake_version;

public:
	UpService();
	~UpService();

	static UpService* getSingleton(void) { return s_singleton; }
private:
	static UpService* s_singleton;
};
