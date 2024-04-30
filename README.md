# CITS3002PROJECT
preview md in vscode: `ctrl+shift+v` • md formatting: https://www.markdownguide.org/cheat-sheet/
## PLANNING
- running on a single computer, all URLs refer to the hostname *localhost*
- each station is its own port
### WHAT IS NEEDED
- program in c
- program in python
- simple HTML page for interface to accept queries to find the sequence of buses and trains from station to station
    - use data from transperth
### FUNCTIONS
- [ ] read, store and update timetable files
- [ ] design a protocol for the servers to communicate with
- [ ] establish connection with client
- [ ] establish connection with other stations
- [ ] calculate all routes from a start to destination at request and find the fastest route
- [ ] handle HTTP requests
- [ ] account for if a bus or train breaks down
- [ ] each station server must support queries from web browser using TCP/IP
- [ ] each station server must also communicate with other station servers using UDP/IP 
