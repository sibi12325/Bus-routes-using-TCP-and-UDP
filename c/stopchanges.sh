pkill -f station-server
netstat -tulpn

#useful debugging commands
#cat /var/log/syslog | grep "station-server\["
#gdb station-server /mnt/wslg/dumps/core.station-server