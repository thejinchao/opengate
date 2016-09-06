#pragma once

enum { GAME_MESSAGE_HEAD_SIZE = 8 };
enum { GATE_MESSAGE_ID_BEGIN = 100, GATE_MESSAGE_ID_END = 1000 };
enum { MAX_GATE_MESSAGE_LENGTH = 1024 };

enum {
	RequestNewAgentID = 100,
	ReplyNewAgentID,
	ForwordMessageUpID,
	ForwordMessageDownID,
	DeleteAgentUpID,
	DeleteAgentDownID,
	ReconnectUpStreamID,
	DebugCommandID,
};

//DownService -> AgentService -> UpService
struct RequestNewAgent
{
	enum { ID = RequestNewAgentID };

	int32_t down_thread_index;
	int32_t connection_id;

	int32_t agent_thread_index;
	int32_t agent_id;

	dhkey_t public_key;
};

//UpService -> AgentService -> DownService
struct ReplyNewAgent
{
	enum { ID = ReplyNewAgentID };

	int32_t up_thread_index;

	int32_t agent_thread_index;
	int32_t agent_id;

	int32_t down_thread_index;
	int32_t connection_id;

	dhkey_t public_key;
};


//DownService -> AgentService ->UpService
struct ForwordMessageUp
{
	enum { ID = ForwordMessageUpID };

	int32_t	agent_thread_index;
	int32_t agent_id;

	int32_t up_thread_index;

	Packet* packet;
};

//UpService->AgentService->DownService
struct ForwordMessageDown
{
	enum { ID = ForwordMessageDownID };

	int32_t	agent_thread_index;
	int32_t agent_id;

	int32_t down_thread_index;
	int32_t connection_id;

	Packet* packet;
};

//DownService -> AgentService ->UpService
struct DeleteAgentUp
{
	enum { ID = DeleteAgentUpID };

	int32_t	agent_thread_index;
	int32_t agent_id;

	int32_t up_thread_index;
};

//UpService->AgentService->DownService
struct DeleteAgentDown
{
	enum { ID = DeleteAgentDownID };

	int32_t	agent_thread_index;
	int32_t agent_id;

	int32_t down_thread_index;
	int32_t connection_id;
};

struct ReconnectUpStream
{
	enum { ID = ReconnectUpStreamID };

	int32_t sleep_time;
};

struct DebugCommand
{
	enum { ID = DebugCommandID };

	int32_t reserved;
};
