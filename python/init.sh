# initialise everything

# kill all processes
pkill -f station-server 
killall station*

# cleanup
find . -name 'tt-*' -exec rm {} \;
rm python/station-server.sh
rm python/mywebpage.html

./buildrandomtimetables 3 # adjust number for timetables amount

./assignports.sh adjacency station-server.sh

./makewebpage.sh station-server.sh mywebpage.html

# then run ./station-server.sh to start the server