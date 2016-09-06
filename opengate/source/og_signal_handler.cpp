#include "og_stdafx.h"
#include "og_signal_handler.h"

#ifndef CY_SYS_WINDOWS

#include <signal.h>
#include <execinfo.h>
#include <sys/types.h>

#include <string>

//-------------------------------------------------------------------------------------
//Install type
#define IT_INVALID		(0)
#define IT_DEFAULT		(1)
#define IT_IGNORE		(2)
#define IT_CUSTOM		(3)

//-------------------------------------------------------------------------------------
//Handle type
#define HT_INVALID		(0)
#define HT_HOLD			(1)
#define HT_RERAISE		(2)

//-------------------------------------------------------------------------------------
static terminal_system_function g_terminal_system_function = 0;

//-------------------------------------------------------------------------------------
struct signal_desc
{
	int32_t			id;
	const char		name[32];
	int32_t			install_type;		//one value of IT_*
	int32_t			handle_type;		//one value	of PT_*
};

//-------------------------------------------------------------------------------------
static signal_desc s_signal_desc[] =
{
	//POSIX.1-1990
	{ SIGHUP, "SIGHUP",		IT_CUSTOM,	HT_HOLD },
	{ SIGINT, "SIGINT",		IT_CUSTOM,	HT_HOLD },
	{ SIGQUIT, "SIGQUIT",	IT_CUSTOM,	HT_RERAISE },
	{ SIGILL, "SIGILL",		IT_CUSTOM,	HT_RERAISE },
	{ SIGABRT, "SIGABRT",	IT_CUSTOM,	HT_RERAISE },
	{ SIGFPE, "SIGFPE",		IT_CUSTOM,	HT_HOLD/*RERAISE*/ },
	{ SIGKILL, "SIGKILL",	IT_INVALID, HT_INVALID },
	{ SIGSEGV, "SIGSEGV",	IT_CUSTOM,	HT_RERAISE },
	{ SIGPIPE, "SIGPIPE",	IT_CUSTOM,	HT_HOLD },
	{ SIGALRM, "SIGALRM",	IT_CUSTOM,	HT_HOLD },
	{ SIGTERM, "SIGTERM",	IT_CUSTOM,	HT_HOLD },
	{ SIGUSR1, "SIGUSR1",	IT_CUSTOM,	HT_HOLD },
	{ SIGUSR2, "SIGUSR2",	IT_CUSTOM,	HT_HOLD },
	{ SIGCHLD, "SIGCHLD",	IT_CUSTOM,	HT_HOLD },
	{ SIGCONT, "SIGCONT",	IT_INVALID, HT_INVALID },
	{ SIGSTOP, "SIGSTOP",	IT_INVALID, HT_INVALID },
	{ SIGTSTP, "SIGTSTP",	IT_CUSTOM,	HT_HOLD },
	{ SIGTTIN, "SIGTTIN",	IT_CUSTOM,	HT_HOLD },
	{ SIGTTOU, "SIGTTOU",	IT_CUSTOM,	HT_HOLD },

	//SUSv2 and POSIX.1-2001
	{ SIGBUS, "SIGBUS",			IT_CUSTOM, HT_RERAISE },
	{ SIGPROF, "SIGPROF",		IT_CUSTOM, HT_HOLD },
	{ SIGSYS, "SIGSYS",			IT_CUSTOM, HT_RERAISE },
	{ SIGTRAP, "SIGTRAP",		IT_CUSTOM, HT_RERAISE },
	{ SIGURG, "SIGURG",			IT_CUSTOM, HT_HOLD },
	{ SIGVTALRM, "SIGVTALRM",	IT_CUSTOM, HT_HOLD },
	{ SIGXCPU, "SIGXCPU",		IT_CUSTOM, HT_HOLD/*RERAISE*/ },
	{ SIGXFSZ, "SIGXFSZ",		IT_CUSTOM, HT_HOLD/*RERAISE*/ },

	//Other
	{ SIGSTKFLT, "SIGSTKFLT",	IT_CUSTOM, HT_HOLD },
	{ SIGIO, "SIGIO",			IT_CUSTOM, HT_HOLD },
	{ SIGPWR, "SIGPWR",			IT_CUSTOM, HT_HOLD },
	{ SIGWINCH, "SIGWINCH",		IT_CUSTOM, HT_HOLD }
};

//-------------------------------------------------------------------------------------
static void _signal_dump_stack(const tm* now, const char* signal_name, const char* handle_type, std::string& content)
{
	content.clear();

	char temp[256] = { 0 };
	snprintf(temp, 256, 
		"<---------\n"
		"TIME=%.4d-%.2d-%.2d_%.2d:%.2d:%.2d\n"
		"SIGNAL=%s\n"
		"SIGNAL_HANDLE_TYPE=%s\n"
		"PROCESS_NAME=open gate\n"
		"PID=%d\n"
		"stack=", 
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec,
		signal_name, handle_type, getpid());
	content = temp;

	void *stack_array[256];
	int32_t stack_array_size = backtrace(stack_array, 256);
	const char **stack_symbols = (const char **)backtrace_symbols(stack_array, stack_array_size);

	if (stack_symbols != 0)
	{
		for (int32_t i = 0; i < stack_array_size; i++)
		{
			content += stack_symbols[i];
			content += "\n";
		}
		free(stack_symbols);
	}

	content += "--------->\n";
}

//-------------------------------------------------------------------------------------
static void _sigal_write_log(const tm* now, const std::string &content)
{
	char filename[256] = { 0 };
	snprintf(filename, sizeof(filename), "./logs/signal.%.4d-%.2d-%.2d-%.2d.log",
		now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour);
	filename[sizeof(filename) - 1] = '\0';

	try
	{
		FILE *fp = fopen(filename, "a+");
		if (fp == 0) return;
		
		fwrite(content.c_str(), 1, content.length(), fp);
		fclose(fp);
	}
	catch (...)
	{
	}
}

//-------------------------------------------------------------------------------------
static void _custom_hadle_function(int32_t signal_id)
{
	size_t signal_counts = sizeof(s_signal_desc) / sizeof(s_signal_desc[0]);
	const signal_desc* s = 0;
	for (size_t i = 0; i < signal_counts; i++) {
		if (s_signal_desc[i].id == signal_id) {
			s = &(s_signal_desc[i]);
			break;
		}
	}
	if (s == 0 || s->install_type != IT_CUSTOM) return;

	//current time
	time_t t = time(0);
	struct tm now;
	localtime_r(&t, &now); 

	//signal we need handle
	if (s->handle_type == HT_HOLD)
	{
		std::string stack;
		_signal_dump_stack(&now, s->name, "HOLD", stack);

		//write to signal log
		_sigal_write_log(&now, stack);

		//quit signal
		if (SIGUSR1 == signal_id) {
			//TODO: quit the system
			if(g_terminal_system_function) {
				g_terminal_system_function();
			}
		}
	}
	else if (s->handle_type == HT_RERAISE)
	{
		std::string stack;
		_signal_dump_stack(&now, s->name, "HOLD", stack);

		//write to signal log
		_sigal_write_log(&now, stack);

		//uninstall signal handle
		signal(signal_id, SIG_DFL);

		//rethrow the signal
		raise(signal_id);
	}

}

#endif

//-------------------------------------------------------------------------------------
void install_signal_handler(terminal_system_function func)
{
#ifdef CY_SYS_WINDOWS
	(void)func;
#else
	size_t signal_counts = sizeof(s_signal_desc) / sizeof(s_signal_desc[0]);
	for (size_t i = 0; i < signal_counts; i++) {
		const signal_desc& s = s_signal_desc[i];

		switch (s.install_type) {
		case IT_DEFAULT:
			signal(s.id, SIG_DFL);
			break;

		case IT_IGNORE:
			signal(s.id, SIG_IGN);
			break;

		case IT_CUSTOM:
			signal(s.id, _custom_hadle_function);
			break;

		case IT_INVALID:
		default:
			break;
		}
	}

	g_terminal_system_function = func;
#endif
}

//-------------------------------------------------------------------------------------
void signal_raise_crash_sample(int32_t index)
{
	switch (index)
	{
	//zero point access
	case 0:
	{
		int32_t *p = 0;
		*p = 1;
	}
	break;

	case 1:
	{
		int32_t *p = 0;
		p += 10;
		*p = 1;
	}
	break;

	case 2:
	{
		int32_t *p = 0;
		p += 100;
		*p = 1;
	}
	break;

	//invalid memory access
	case 3:
	{
		int32_t *p = (int32_t *)12345678;
		*p = 1;
	}
	break;

	case 4:
	{
		int32_t *p = new int32_t;
		delete p;
		delete p;
	}
	break;

	case 5:
	{
		int32_t *p = new int32_t;
		delete p;
		*p = 1;
	}
	break;

	//over boundary
	case 6:
	{
		char szTemp[128];
		for (int32_t i = 0; i < 1000000; i++)
		{
		szTemp[i] = 0;
		}
	}
	break;

	case 7:
	{
		char *p = new char[128];
		for (int32_t i = 0; i < 1000000; i++)
		{
			p[i] = 0;
		}
	}
	break;

	//recursive
	case 8:
	{
		signal_raise_crash_sample(index);
	}
	break;

	//divide zero
	case 9:
	{
		int32_t i = 0;
		int32_t n = 10 / i;
		(void)n;
	}
	break;

	//modify static memory
	case 10:
	{
		const char *p = "1234567890";
		char* wp = (char*)p;
		*wp = 0;
	}
	break;

	}
	
}

