gcc -std=c11 -Wall -Werror -g -o station-server station-server.c
./assignports.sh adjacency station-server.sh 
./makewebpage.sh station-server.sh mywebpage.html 
echo
./station-server.sh