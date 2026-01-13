#!/bin/sh

pkill -f "Xephyr.*:1" 2>/dev/null
rm -f /tmp/.X1-lock

Xephyr :1 -screen 1280x720 -ac &
sleep 0.5

DISPLAY=:1 ./bin/qwm
#DISPLAY=:1 valgrind --leak-check=summary --log-file=memcheck.txt ./bin/qwm
