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

## PYTHON TODO:
- [ ] Python: TCP connection to client
- [ ] Python: UDP connection to another server
- [ ] Python: Read/Store/Update station timetables <--- Get to here by week 10
- [ ] Python: Implement a protocol for communication <--- Discuss this week 10 meeting
- [ ] Python: Convert client requests to server format
- [ ] Python: Convert server responses to client format
- [ ] Python: Communicate between multiple computers
- [ ] Python: Find valid, one distance, route
- [ ] Python: Find valid, multi stop, routes
- [ ] Python: Find valid routes with cycles present
- [ ] Python: Find non-valid routes
- [ ] Python: Find the fastest route (optional?)
- [ ] Python: Be able to handle real data, 20 stations
- [ ] Python: Be able to handle multiple outgoing requests at once

## C TODO:
- [X] C: TCP connection to client
- [X] C: UDP connection to another server
- [X] C: Read/Store/Update station timetables 
- [X] C: Implement a protocol for communication 
- [ ] C: Communicate between multiple computers
- [X] C: Find valid, one distance, route
- [X] C: Find valid, multi stop, routes
- [X] C: Find valid routes with cycles present
- [ ] C: Find non-valid routes
- [X] C: Find the fastest route (optional?)
- [ ] C: Be able to handle real data, 20 stations
- [ ] C: Be able to handle multiple outgoing requests at once
