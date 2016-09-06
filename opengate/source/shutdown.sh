#!/bin/sh


CURR_DIR=`pwd`
PID_FILE="$CURR_DIR/logs/opengate.pid"

if [ -f "$PID_FILE" ]; then
        opengate_pid=`cat $PID_FILE`
	kill -USR1 $opengate_pid
	echo "send USR1 to opengate process, pid=${opengate_pid}"

	rm -f $PID_FILE
else
	echo "WRONG! opengate is not running!"
fi

