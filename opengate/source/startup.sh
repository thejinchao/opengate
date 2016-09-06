#! /bin/sh

CURR_DIR=`pwd`
PID_FILE="$CURR_DIR/logs/opengate.pid"

if [ -f "$PID_FILE" ]; then
	opengate_pid=`cat $PID_FILE`
	echo "WRONG! opengate is running, pid=${opengate_pid}"

else
	./opengate > /dev/null &
	echo "opengate startup..."
fi
 
