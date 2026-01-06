#!/bin/sh

# dbus-run-session -- sh <<'EOF'

pkill -f "Xephyr.*:1" 2>/dev/null
rm -f /tmp/.X1-lock

Xephyr :1 -screen 800x600 -ac &
sleep 0.5
#DISPLAY=:1 ./bin/qwm

DISPLAY=:1 valgrind --leak-check=summary --log-file=memcheck.txt ./bin/qwm

#DISPLAY=:1 gdb -q -ex "set pagination off" \
               #-ex "catch throw" \
               #-ex "run" \
               #--args ./bin/qwm 2>&1 | tee /tmp/qwm_gdb.log

# EOF
