#pragma once


typedef void(*terminal_system_function)(void);

void install_signal_handler(terminal_system_function fun);

/*
	carsh test 
	[0, 2]: zero point access
	[3, 5]: invalid memory access
	[6, 7]:	over boundary
	[8]:	recursive
	[9]:	divide zero
	[10]:	modify static memory
*/
void signal_raise_crash_sample(int32_t index);
