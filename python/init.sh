# initialise everything

./buildrandomtimetables 3 # adjust number for timetables amount

./assignports.sh adjacency station-server.sh

./makewebpage.sh station-server.sh mywebpage.html

# then run ./station-server.sh to start the server