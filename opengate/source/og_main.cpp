#include "og_stdafx.h"
#include "og_config.h"
#include "og_agent_service.h"
#include "og_down_service.h"
#include "og_up_service.h"
#include "og_signal_handler.h"
#include "og_debug.h"

//-------------------------------------------------------------------------------------
bool process_write_pid_file(void)
{
#ifdef CY_SYS_WINDOWS
	return true;
#else
	pid_t process_id = ::getpid();
	FILE* fp = fopen("./logs/opengate.pid", "w");
	if (fp == 0) return false;
	fprintf(fp, "%d\n", process_id);
	fclose(fp);
	return true;
#endif
}

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	//install signal handle
	install_signal_handler(DownService::signal_terminate);

	//init socket api(windows)
	socket_api::global_init();
	CY_LOG(L_DEBUG, "Open gate system start, build time %s, cpu_counts=%d, jemalloc=%s", 
		BUILD_TIME, sys_api::get_cpu_counts(), 
#ifdef  CY_HAVE_JEMALLOC_LIBRARIES
		"enable"
#else
		"disable"
#endif
		);

	//load config from config file
	Config config;
	if (!config.load("opengate.ini")) {
		return 1;
	}
	CY_LOG(L_DEBUG, "load config file success, handshake_version=%d!", config.get_down_stream().handshake_ver);

	//init debug threads
	Debug debuger;
	if (config.is_debug_enable()) {
		debuger.init_redis_thread(config.get_redis_host().c_str(), (uint16_t)config.get_redis_port(),
			sys_api::get_cpu_counts());
	}

	//create agent service
	AgentService agentService;
	agentService.start(sys_api::get_cpu_counts(), &config);

	//create up service
	UpService upService;
	upService.start(&config);

	//create down service
	DownService downService;
	if (!downService.start(&config, &debuger)) {
		CY_LOG(L_FATAL, "Create down service failed!");
		return 1;
	}

	//write pid file
	if (!process_write_pid_file()) {
		CY_LOG(L_ERROR, "write pid file failed!");
		return 1;
	}

	//init monitor thread
	if (config.is_debug_enable() && debuger.is_enable()) {
		debuger.init_monitor_thread(config.get_debug_fraq());
	}

	//make every thing work...
	downService.join();
	return 0;
}
