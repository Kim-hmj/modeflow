#!/bin/sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
adk-message-send 'system_mode_management{name:"misc::logdebug"}'
if [ $# -eq 0 ]; then
tail -n300 -F /var/log/r1-modeflow  2> /dev/null |stdbuf -oL grep mfl: |egrep -v "_perf_us|rtc|charging|Timer|Thread::|gpio::detect"
elif [ $1 == "notimer" ]; then
tail -n300 -F /var/log/r1-modeflow  2> /dev/null |stdbuf -oL grep mfl: |egrep -v "rtc|_perf_us|DockEnI2C5|checkBat|bat_checked|charging|Timer|Thread::|connectivity_bt_blecmd|connectivity_bt_bleexec|wantStartButWlanNotConnected|ota::|rauc-hawkbit-updater"
elif [ $1 == "internal" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep sendP  2> /dev/null
elif [ $1 == "adkin" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep rcvadk 2> /dev/null
elif [ $1 == "hwin" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep -E '(gpio::[0-9].+|PwrDlvr::[~d].+)' 2> /dev/null
elif [ $1 == "timing" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep -E '(Thread::.+|Timer::.+)' 2> /dev/null
elif [ $1 == "in" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep -E '(rcvadk|sndp|sndtm|gpio::[0-9].+|PwrDlvr::[~d].+|Thread::.+)'  2> /dev/null
elif [ $1 == "out" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep -E 'sndadk' 2> /dev/null
elif [ $1 == "external" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep -E 'sndadk|rcvadk|gpio::[0-9].+|PwrDlvr::[~d].+|exec.+' 2> /dev/null
elif [ $1 == "perf" ]; then
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|stdbuf -oL grep mfl: |grep -E '_perf_us' 2> /dev/null
else
stdbuf -oL tail -n300 -F /var/log/r1-modeflow  2> /dev/null|grep "mfl:" 2> /dev/null
fi
adk-message-send 'system_mode_management{name:"misc::logrelease"}'
