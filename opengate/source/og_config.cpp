#include "og_stdafx.h"

#include "og_config.h"


/*
https://github.com/Blandinium/inih
*/

#define INI_ALLOW_MULTILINE 1
#define INI_USE_STACK 1
#define INI_ALLOW_BOM 1
#define INI_STOP_ON_FIRST_ERROR 1
#define INI_MAX_LINE 200

#define MAX_SECTION 50
#define MAX_NAME 50

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
	char* p = s + strlen(s);
	while (p > s && isspace((unsigned char)(*--p)))
		*p = '\0';
	return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
	while (*s && isspace((unsigned char)(*s)))
		s++;
	return (char*)s;
}

/* Return pointer to first char c or ';' comment in given string, or pointer to
null at end of string if neither found. ';' must be prefixed by a whitespace
character to register as a comment. */
static char* find_char_or_comment(const char* s, char c)
{
	int was_whitespace = 0;
	while (*s && *s != c && !(was_whitespace && *s == ';')) {
		was_whitespace = isspace((unsigned char)(*s));
		s++;
	}
	return (char*)s;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
static char* strncpy0(char* dest, const char* src, size_t size)
{
	strncpy(dest, src, size);
	dest[size - 1] = '\0';
	return dest;
}

/* See documentation in header file. */
int ini_parse_file(FILE* file,
	int(*handler)(void*, const char*, const char*,
	const char*),
	void* user)
{
	/* Uses a fair bit of stack (use heap instead if you need to) */
#if INI_USE_STACK
	char line[INI_MAX_LINE];
#else
	char* line;
#endif
	char section[MAX_SECTION] = "";
	char prev_name[MAX_NAME] = "";

	char* start;
	char* end;
	char* name;
	char* value;
	int lineno = 0;
	int error = 0;

#if !INI_USE_STACK
	line = (char*)malloc(INI_MAX_LINE);
	if (!line) {
		return -2;
	}
#endif

	/* Scan through file line by line */
	while (fgets(line, INI_MAX_LINE, file) != NULL) {
		lineno++;

		start = line;
#if INI_ALLOW_BOM
		if (lineno == 1 && (unsigned char)start[0] == 0xEF &&
			(unsigned char)start[1] == 0xBB &&
			(unsigned char)start[2] == 0xBF) {
			start += 3;
		}
#endif
		start = lskip(rstrip(start));

		if (*start == ';' || *start == '#') {
			/* Per Python ConfigParser, allow '#' comments at start of line */
		}
#if INI_ALLOW_MULTILINE
		else if (*prev_name && *start && start > line) {
			/* Non-black line with leading whitespace, treat as continuation
			of previous name's value (as per Python ConfigParser). */
			if (!handler(user, section, prev_name, start) && !error)
				error = lineno;
		}
#endif
		else if (*start == '[') {
			/* A "[section]" line */
			end = find_char_or_comment(start + 1, ']');
			if (*end == ']') {
				*end = '\0';
				strncpy0(section, start + 1, sizeof(section));
				*prev_name = '\0';

				if (!handler(user, section, 0, 0) && !error)
					error = lineno;
			}
			else if (!error) {
				/* No ']' found on section line */
				error = lineno;
			}
		}
		else if (*start && *start != ';') {
			/* Not a comment, must be a name[=:]value pair */
			end = find_char_or_comment(start, '=');
			if (*end != '=') {
				end = find_char_or_comment(start, ':');
			}
			if (*end == '=' || *end == ':') {
				*end = '\0';
				name = rstrip(start);
				value = lskip(end + 1);
				end = find_char_or_comment(value, '\0');
				if (*end == ';')
					*end = '\0';
				rstrip(value);

				/* Valid name[=:]value pair found, call handler */
				strncpy0(prev_name, name, sizeof(prev_name));
				if (!handler(user, section, name, value) && !error)
					error = lineno;
			}
			else if (!error) {
				/* No '=' or ':' found on name[=:]value line */
				error = lineno;
			}
		}

#if INI_STOP_ON_FIRST_ERROR
		if (error)
			break;
#endif
	}

	//the end
	if (!handler(user, 0, 0, 0) && !error)
		error = lineno;

#if !INI_USE_STACK
	free(line);
#endif

	return error;
}

/* See documentation in header file. */
int ini_parse(const char* filename,
	int(*handler)(void*, const char*, const char*, const char*),
	void* user)
{
	FILE* file;
	int error;

	file = fopen(filename, "r");
	if (!file)
		return -1;
	error = ini_parse_file(file, handler, user);
	fclose(file);
	return error;
}


//-------------------------------------------------------------------------------------
Config::Config()
	: m_enable_packet_encrypt(false)
	, m_enable_packet_log(false)
	, m_down_stream_loaded(false)
	, m_up_stream_loaded(false)
	, m_cpu_counts(0)
{
}

//-------------------------------------------------------------------------------------
Config::~Config()
{

}

//-------------------------------------------------------------------------------------
bool Config::_config_handler(const char* section, const char* name, const char* value)
{
	#define MATCH_SECTION(s) (name==0 && value==0 && strcmp(section, s)==0)
	#define MATCH(s, n) (strcmp(section, s) == 0 && strcmp(name, n) == 0)

	if (section == 0) {
		//check
		if (!m_down_stream_loaded) return false;

		if (m_down_stream.server_ip == "" ||
			m_down_stream.server_port == 0 ||
			m_down_stream.handshake_ver == 0) return false;

		if (m_down_stream.thread_counts == 0)
			m_down_stream.thread_counts = _cpu_counts();


		if (!m_up_stream_loaded) return false;

		if (m_up_stream.server_ip == "" ||
			m_up_stream.server_port == 0) return false;

		if (m_up_stream.thread_counts == 0)
			m_up_stream.thread_counts = _cpu_counts();

	}
	else if (MATCH_SECTION("down_stream")) {
		if (m_down_stream_loaded) {
			return false;
		}
		m_down_stream_loaded = true;
	}
	else if (MATCH("down_stream", "server_ip")) {
		m_down_stream.server_ip = _get_as_string(value);
	}
	else if (MATCH("down_stream", "server_port")) {
		m_down_stream.server_port = _get_as_int(value);
	}
	else if (MATCH("down_stream", "handshake_ver")) {
		m_down_stream.handshake_ver = _get_as_int(value);
	}
	else if (MATCH_SECTION("up_stream")) {
		if (m_up_stream_loaded) {
			return false;
		}
		m_up_stream_loaded = true;
	}
	else if (MATCH("up_stream", "server_ip")) {
		m_up_stream.server_ip = _get_as_string(value);
	}
	else if (MATCH("up_stream", "server_port")) {
		m_up_stream.server_port = _get_as_int(value);
	}
	else if (MATCH("up_stream", "thread_counts")) {
		m_up_stream.thread_counts = _get_as_int(value);
	}
	else if (MATCH_SECTION("global")) {
	}
	else if (MATCH("global", "enable_packet_log")) {
		m_enable_packet_log = (_get_as_int(value) != 0);
	}
	else if (MATCH("global", "enable_debug")) {
		m_enable_debug = (_get_as_int(value) != 0);
	}
	else if (MATCH("global", "redis_host")) {
		m_redis_host = _get_as_string(value);
	}
	else if (MATCH("global", "redis_port")) {
		m_redis_port = _get_as_int(value);
	}
	else if (MATCH("global", "debug_fraq")) {
		m_debug_fraq = _get_as_int(value);
	}
	else if (MATCH("global", "enable_packet_encrypt")) {
		m_enable_packet_encrypt = (_get_as_int(value) != 0);
	}

	return true;
}

//-------------------------------------------------------------------------------------
void Config::clear()
{
	m_enable_packet_log = false;
	m_enable_debug = false;

	m_down_stream_loaded = false;
	m_down_stream.server_ip = "";
	m_down_stream.server_port = 0;
	m_down_stream.thread_counts = 0;

	m_up_stream_loaded = false;
	m_up_stream.server_ip = "";
	m_up_stream.server_port = 0;
	m_up_stream.thread_counts = 0;
}

//-------------------------------------------------------------------------------------
bool Config::load(const char* config_file)
{
	clear();

	int err_line = ini_parse(config_file, _config_handler_entry, this);

	if (err_line!=0) {
		CY_LOG(L_ERROR, "parse config file error! line_number=%d", err_line);
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
const char* _try_get_env(const char* value) {
	if (strlen(value) > 5 && value[0] == '$' && value[1] == '(') {
		const char* end = strchr(value, ')');
		if (end == 0) return 0;

		char temp[1024] = { 0 };
		strncpy(temp, value + 2, end - value - 2);

		const char* env_value = getenv(temp);
		if (env_value != 0 && env_value[0] != 0) return env_value;

		return end + 1;
	}

	return 0;
}

//-------------------------------------------------------------------------------------
int32_t	Config::_get_as_int(const char* value)
{
	const char* env_value = _try_get_env(value);
	if (env_value) return atoi(env_value);

	return atoi(value);
}

//-------------------------------------------------------------------------------------
std::string Config::_get_as_string(const char* value)
{
	const char* env_value = _try_get_env(value);
	if (env_value) return std::string(env_value);

	return std::string(value);
}

//-------------------------------------------------------------------------------------
uint32_t Config::_cpu_counts(void)
{
	if (m_cpu_counts != 0) return m_cpu_counts;

	const uint32_t DEFAULT_CPU_COUNTS = 2;

#ifdef CY_SYS_WINDOWS
	//use GetLogicalProcessorInformation

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	DWORD returnLength = 0;
	while (FALSE == GetLogicalProcessorInformation(buffer, &returnLength))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			if (buffer) free(buffer);
			buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
		}
		else
		{
			if (buffer) free(buffer);
			return DEFAULT_CPU_COUNTS;
		}
	}

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION p = buffer;
	DWORD byteOffset = 0;
	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) 
	{
		if(p->Relationship==RelationProcessorCore) {
			m_cpu_counts++;
		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		p++;
	}
	free(buffer);
	return m_cpu_counts;
#else
	long int ncpus;
	if ((ncpus = sysconf(_SC_NPROCESSORS_ONLN)) == -1){
		CY_LOG(L_ERROR, "get cpu counts \"sysconf(_SC_NPROCESSORS_ONLN)\" error, use default(2)");
		return DEFAULT_CPU_COUNTS;
	}
	return m_cpu_counts=(uint32_t)ncpus;
#endif
}

