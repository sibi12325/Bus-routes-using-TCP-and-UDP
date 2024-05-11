<<<<<<< HEAD
=======
# initialise everything

pkill -f station-server # kill all processes

# cleanup
find . -name 'tt-*' -exec rm {} \;
rm python/station-server.sh
rm python/mywebpage.html

>>>>>>> a8944fe23352e1a3879f9eac3c70ac656ec5955d
./buildrandomtimetables 3 # adjust number for timetables amount

./assignports.sh adjacency station-server.sh

./makewebpage.sh station-server.sh mywebpage.html

# then run ./station-server.sh to start the server