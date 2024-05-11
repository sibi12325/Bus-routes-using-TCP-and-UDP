#!/usr/bin/python3


import sys
import socket
import time
import datetime
import select
import hashlib

IP = 'localhost'


def parse_destination(query):
    # Split the query into lines
    lines = query.split('\n')
    # Look for the line containing the destination
    for line in lines:
        if line.startswith('GET'):
            path = line.split()[1]
            # Check if the path contains the destination
            if path.startswith('/?to='):
                # Extract the destination
                destination = path.split("?to=")[1]
                return destination
    # If no destination is found, return None
    return None

def read_timetable(filename):
    timetable = {}
    with open(filename, 'r') as file:
        # Skip lines until we find the first departure information
        line = file.readline()
        while line:
            # Skip lines starting with '#'
            if not line.startswith('#'):
                parts = line.strip().split(',')
                # Check if this line contains departure information
                if len(parts) == 5:
                    destination = parts[4]
                    departure_info = (parts[0], parts[1], parts[2], parts[3])
                    if destination in timetable:
                        timetable[destination].append(departure_info)
                    else:
                        timetable[destination] = [departure_info]
            line = file.readline()
    return timetable

def generate_http_response(body):
    response = f"HTTP/1.1 200\r\n"
    response += f"Content-Type: text/html\r\n"
    response += "Connection: Closed\r\n\r\n"
    response += f"{body}"
    return response


def find_fastest_route(timetable, destination, after_time_str):
    if destination not in timetable:
        return None
    
    # Get the timetable entries for the destination
    destination_timetable = timetable[destination]

    # Initialize variables to store the fastest route information
    fastest_duration = None
    fastest_route = None

    # Parse the after_time string into a datetime object
    after_time = datetime.datetime.strptime(after_time_str, "%H:%M")

    # Iterate through the timetable entries
    for entry in destination_timetable:
        # Parse the departure and arrival time strings into datetime objects
        departure_time = datetime.datetime.strptime(entry[0], "%H:%M")
        arrival_time = datetime.datetime.strptime(entry[3], "%H:%M")
        
        if(departure_time > after_time):
            # Calculate the duration of the route, including waiting time
            duration = departure_time - after_time + (arrival_time - departure_time)

            # Check if this is the fastest route found so far
            if fastest_duration is None or duration < fastest_duration:
                fastest_duration = duration
                fastest_route = entry

    return fastest_route


def send_udp_own_station(client_fd, destination, station_name, query_port):
    initial_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    msg = f"Q~{station_name}~{destination}~{client_fd}"
    initial_socket.sendto(msg.encode(), (IP, query_port))
    initial_socket.close()

def calculate_file_hash(file_name):
    """Calculate the SHA-256 hash of a file."""
    hasher = hashlib.sha256()
    with open(file_name, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            hasher.update(chunk)
    return hasher.hexdigest()

def server(station_name, browser_port, query_port, neighbours):

    # Convert port numbers to integers
    browser_port = int(browser_port)
    query_port = int(query_port)

    # Create a TCP/IP socket for handling queries from the web interface
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.bind((IP, browser_port))
    tcp_socket.listen(5)
    print(f"{station_name} TCP Server listening on {IP}:{browser_port}")
        
    # Create a UDP socket for handling queries from the stations
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind((IP, query_port))
    print(f"{station_name} UDP Server listening on {IP}:{query_port}")

    #Read the timetable
    filename = f"tt-{station_name}"
    timetable = read_timetable(filename)
    hash = calculate_file_hash(filename)
    # Create a poll object
    poll_object = select.poll()
    poll_object.register(tcp_socket, select.POLLIN)
    poll_object.register(udp_socket, select.POLLIN)

    #Dictionary of client sockets 
    client_sockets = {}

    while True:
        events = poll_object.poll()
        for fd, event in events:
            # TCP connection
            if fd == tcp_socket.fileno() and event & select.POLLIN:
                connection, address = tcp_socket.accept()
                request = connection.recv(1024).decode()
                print(f"New TCP connection from {address}")

                if(request.startswith("GET")):
                    # Store the client socket and its file descriptor in the dictionary
                    client_fd = connection.fileno()
                    client_sockets[client_fd] = connection
                    poll_object.register(connection, select.POLLOUT)
                    #calculate new hash
                    new_hash = calculate_file_hash(filename)
                    #if the old hash is different to new hash then update the timetable
                    if(hash != new_hash):
                        timetable = read_timetable(filename)
                        hash = new_hash
                    #get the destination if the source and destination are not connected
                    destination = parse_destination(request)
                    t = time.localtime()
                    current_time = time.strftime("%H:%M", t)
                    if(destination in timetable):
                        route = find_fastest_route(timetable, destination, current_time)
                        response = generate_http_response(route)
                        connection.sendall(response.encode())
                        poll_object.unregister(connection)
                        del client_sockets[client_fd]
                        connection.close()
                    else:
                        # if the station not connected send it udp server of this station
                        send_udp_own_station(client_fd, destination, station_name, query_port)

                # this will send route to the webpage
                elif (request.startswith("R")):
                    segment = data.decode().split("~")
                    answer = f"Route to {parts[1]} from {station_name}:\n"
                    answer += f"{parts[-1]}"
                    reply = generate_http_response(answer)
                    client_socket = client_sockets.get(int(segment[3]))
                    client_socket.sendall(reply.encode())
                    poll_object.unregister(client_socket)
                    del client_sockets[int(segment[3])]
                    client_socket.close()
                    
                    
            # UDP data
            elif fd == udp_socket.fileno() and event & select.POLLIN:
                data, address = udp_socket.recvfrom(1024)
                print(f"{station_name} UDP data received from {address}: {data.decode()}")
                # Handle UDP data and split into parts
                parts = data.decode().split("~")

                # checks to see if the timetable changed or not
                new_hash = calculate_file_hash(filename)
                if(hash != new_hash):
                    timetable = read_timetable(filename)
                    hash = new_hash

                #If there is route to destination and sends it back
                if(parts[0] == "M" and parts[2] in timetable):
                    t = time.localtime()
                    current_time = time.strftime("%H:%M", t)
                    route = find_fastest_route(timetable, parts[2], current_time)
                    msg = f"R~{station_name}~{parts[1]}~{parts[3]}~{route}"
                    udp_socket.sendto(msg.encode(), address)
                
                #this will send msg back to the tcp server
                elif(parts[0] == "R" and parts[2] == station_name):
                    tcp_send_back = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    tcp_send_back.connect((IP, browser_port))
                    tcp_send_back.sendall(data)
                    tcp_send_back.close()

                # deals with the query sent from it stations own tcp server and sends to neighbours
                elif(parts[0] == "Q" and parts[1] == station_name):
                    ack = False
                    if ack == True:
                        msg_type = "A"
                    else:
                        msg_type = "M"
                    msg = f"{msg_type}~{station_name}~{parts[2]}~{parts[3]}"
                    for neighbour in neighbours:
                        udp_socket.sendto(msg.encode(), neighbour)

                #if the destination is not in the stations timetable then it send its own neighbours
                elif(parts[0] == "M" and parts[2] not in timetable):
                    msg_type = "M"
                    msg = f"{msg_type}~{station_name}~{parts[2]}~{parts[3]}"
                    for neighbour in neighbours:
                        if neighbour != address: #checks to see if neighbour isnt the same as the where msg came from
                            udp_socket.sendto(msg.encode(), neighbour)
                
if __name__ == "__main__":
    # Check if the correct number of command line arguments are provided
    if len(sys.argv) < 5:
        print("Usage: ./station-server.py station-name browser-port query-port neighbor1 [neighbor2 ...]")
        sys.exit(1)

    # Extract command line arguments
    station_name = sys.argv[1]
    browser_port = sys.argv[2]
    query_port = sys.argv[3]
    neighbours_l = sys.argv[4:]

    # Convert neighbors into a list of tuples
    neighbours = [(neighbour.split(":")[0], int(neighbour.split(":")[1])) for neighbour in neighbours_l]

    # start the server
    server(station_name, browser_port, query_port, neighbours)
    