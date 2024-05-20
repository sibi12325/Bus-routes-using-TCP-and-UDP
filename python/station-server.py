#!/usr/bin/python3

# CITS3002 - Computer Networks Project
# Created by
# Cooper Thompson - 23621342 
# Sibi Moothedan - 23615908
# Devarsh Patel - 22964473
# Emily Matthews - 23476614
# Developed for Linux Ubuntu 22.04

import sys
import socket
import time
import select
import hashlib


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
                destination = path.split("?to=")[1].split("&")[0]
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

def getMins(string):
    li = string.split(":")

    if(len(li[1]) > 2):
        li[1] = li[1][0:1]

    mins = int(li[0]) * 60 + int(li[1])
    
    return mins


def find_fastest_route(timetable, destination, after_time_str):
    if destination not in timetable:
        return None
    
    # Get the timetable entries for the destination
    destination_timetable = timetable[destination]

    # Initialize variables to store the fastest route information
    fastest_duration = None
    fastest_route = None

    # Parse the after_time string into a datetime object
    after_time = getMins(after_time_str)

    # Iterate through the timetable entries
    for entry in destination_timetable:
        # Parse the departure and arrival time strings into datetime objects
        departure_time = getMins(entry[0])
        arrival_time = getMins(entry[3])
        
        if(departure_time > after_time):
            # Calculate the duration of the route, including waiting time
            duration = departure_time - after_time + (arrival_time - departure_time)

            # Check if this is the fastest route found so far
            if fastest_duration is None or duration < fastest_duration:
                fastest_duration = duration
                fastest_route = entry
    
    return fastest_route


def send_udp_own_station(IP, client_fd, destination, station_name, query_port, leave_time):
    initial_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    msg = f"Q~{station_name}~{client_fd}~{destination}~{leave_time}"
    initial_socket.sendto(msg.encode(), (IP, query_port))
    initial_socket.close()

def calculate_file_hash(file_name):
    """Calculate the SHA-256 hash of a file."""
    hasher = hashlib.sha256()
    with open(file_name, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            hasher.update(chunk)
    return hasher.hexdigest()

def route_destination_time(route):
    return route[-1]

def extract_time(message):
    # Split the message by "&leave=" to separate the station name and time
    parts = message.split("&leave=")
    
    # Check if "&leave=" exists in the string
    if len(parts) < 2:
        return None  # "&leave=" not found, return None
    
    # Extract the part after "&leave="
    time_string = parts[1].split(" ")[0]
    
    # Replace "%3A" with ":"
    time_string = time_string.replace("%3A", ":")
    
    return time_string

def list_to_string(list):
    if len(list) == 1:
        return str(list[0])
    else:
        return '@'.join(map(str, list))

def extract_final_time(string):
    elements = string.strip("()").split(", ")

    return elements[-1].strip("'")

def add_station(journey, station_name):
    if station_name not in journey:
        journey.insert(0, station_name)  # Add the station to the front of the list
    return journey

def server(station_name, browser_port, query_port, neighbours):

    
    IP = "0.0.0.0"
    # Convert port numbers to integers
    browser_port = int(browser_port)
    query_port = int(query_port)

    # store the neighbour address with corresponding names
    neighbour_address = {}

    # Create a TCP/IP socket for handling queries from the web interface
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.bind((IP, browser_port))
    tcp_socket.listen(5)
    print(f"{station_name} TCP Server listening on {IP}:{browser_port}")
        
    # Create a UDP socket for handling queries from the stations
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind((IP, query_port))
    print(f"{station_name} UDP Server listening on {IP}:{query_port}")

    time.sleep(2)
    # Send station names to its neighbours
    station_to_neighbour = f"I~{station_name}"
    for neighbour in neighbours:
        udp_socket.sendto(station_to_neighbour.encode(), neighbour)
        print(f"Send To {neighbour}: {station_to_neighbour}")


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

    # Dictionary to store timestamps of queries {client_fd: timestamp}
    query_timestamps = {}

    # List of source_ids that have visted this station previously, will be dropped
    visited = []

    message_id = 30

    while True:

        current_time = time.time()
        events = poll_object.poll() 

        # Check for timeouts
        to_remove = []
        for client_fd, timestamp in query_timestamps.items():
            if current_time - timestamp > 5: # change this for timeout duration
                if client_fd in client_sockets:
                    client_socket = client_sockets[client_fd]
                    response = generate_http_response(f"Fastest route to {destination}: No valid route to {destination}") # change message according to c
                    client_socket.sendall(response.encode())
                    poll_object.unregister(client_socket)
                    client_socket.close()
                    del client_sockets[client_fd]
                to_remove.append(client_fd)
        for fd in to_remove:
            del query_timestamps[fd]

        for fd, event in events:
            # TCP connection
            if fd == tcp_socket.fileno() and event & select.POLLIN:
                connection, address = tcp_socket.accept()
                request = connection.recv(1024).decode()
                print(f"New TCP connection from {address}")
                if(request.startswith("GET")):
                    # Store the client socket and its file descriptor in the dictionary
                    message_id += 1
                    client_fd = message_id
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
                    leave_time = extract_time(request)
                    if(destination in timetable):
                        # if the station is connected send the route back to the webpage
                        route = find_fastest_route(timetable, destination, leave_time)
                        answer = f"Fastest route to {destination}: "
                        if (route == None):
                            route = f"No valid route to {destination}"
                        answer += f"{route}"
                        response = generate_http_response(answer)
                        connection.sendall(response.encode())
                        poll_object.unregister(connection)
                        del client_sockets[client_fd]
                        connection.close()
                    elif(destination is not None):
                        # add the current time to the timestamp of the client_fd
                        query_timestamps[client_fd] = current_time
                        # if the station not connected send it udp server of this station
                        send_udp_own_station(IP, client_fd, destination, station_name, query_port, leave_time)
                        

                # this will send route to the webpage
                elif (request.startswith("R")):
                    segment = data.decode().split("~")
                    routes = segment[4].split('@')
                    destination_time = extract_final_time(routes[-1])
                    answer = f"Route to {parts[1]} from {parts[3]}: "
                    for route in routes:
                        answer += f"{route},"
                    reply = generate_http_response(answer)
                    client_socket = client_sockets.get(int(segment[2]))
                    if client_socket:
                        client_socket.sendall(reply.encode())
                        poll_object.unregister(client_socket)
                        del client_sockets[int(segment[2])]
                        client_socket.close()
                    else:
                        pass
                    
                    
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

                # if its an m message, construct the source_id
                if(parts[0] == "M"):
                    source_id = parts[1] + "~" + parts[2]
                    if (source_id in visited):
                        print(f"Dropping packet : {source_id}")
                        continue
                    visited.append(source_id)

                if(parts[0] == "M" and parts[3] == station_name):
                    journey = parts[6].split('@')
                    routes = parts[5].split('@')
                    back_station = journey[0]
                    del journey[0] 
                    journey = list_to_string(journey)
                    routes = list_to_string(routes)
                    msg = f"R~{parts[3]}~{parts[2]}~{parts[1]}~{routes}~{journey}"
                    udp_socket.sendto(msg.encode(), address)
                    print(f"Send to {address} : {msg}")


                elif(parts[0] == "M" and parts[3] in timetable):
                    route = find_fastest_route(timetable, parts[3], parts[4])
                    if route != None:
                        destination_time = route_destination_time(route)
                        if (destination_time < '24:00'):
                            journey = parts[6].split('@')
                            routes = parts[5].split('@')
                            routes.append(route)
                            back_station = journey[0]
                            del journey[0] 
                            journey = list_to_string(journey)
                            routes = list_to_string(routes)
                            if back_station in neighbour_address:
                                msg = f"R~{parts[1]}~{parts[2]}~{parts[3]}~{routes}~{journey}"
                                udp_socket.sendto(msg.encode(), neighbour_address[back_station])
                                print(f"Send to {neighbour_address[back_station]} : {msg}")
                            else:
                                pass
                
                #Recieve the station name and store it
                elif (parts[0] == "I"):
                    neighbour_address[parts[1]] = address


                #this will send msg back to the tcp server
                # need ack
                elif(parts[0] == "R" and parts[1] == station_name):
                    tcp_send_back = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    tcp_send_back.connect((IP, browser_port))
                    tcp_send_back.sendall(data)
                    tcp_send_back.close()

                # deals with the query sent from it stations own tcp server and sends to neighbours
                # need ack
                elif(parts[0] == "Q" and parts[1] == station_name):
                    for neighbour in neighbour_address.keys():
                        route = find_fastest_route(timetable, neighbour, parts[4])
                        if route != None and route_destination_time(route) < '24:00':
                            destination_time = route_destination_time(route)
                            msg = f"M~{station_name}~{parts[2]}~{parts[3]}~{destination_time}~{route}~{station_name}"
                            udp_socket.sendto(msg.encode(), neighbour_address[neighbour])
                            print(f"Send to {neighbour_address[neighbour]} : {msg}")

                # if the destination is not in the stations timetable then it send its own neighbours
                # need ack
                elif(parts[0] == "M" and parts[3] not in timetable):
                    if len(neighbour_address) == 1:
                        continue  # Skip the block if there is only one neighbor
                    station_journey = parts[6].split('@')
                    station_routes = parts[5].split('@')
                    found_valid_route = False
                    for neighbour in neighbour_address.keys():
                        individual_routes = station_routes[:]
                        if neighbour_address[neighbour] == address: #checks to see if neighbour isnt the same as the where msg came from
                            continue
                        route = find_fastest_route(timetable, neighbour, parts[4])
                        if route != None and route_destination_time(route) < '24:00':
                            individual_journey = add_station(station_journey, station_name)
                            individual_routes.append(route)
                            sent_routes = list_to_string(individual_routes)
                            sent_journey = list_to_string(individual_journey)
                            destination_time = route_destination_time(route)
                            msg = f"M~{parts[1]}~{parts[2]}~{parts[3]}~{destination_time}~{sent_routes}~{sent_journey}"
                            udp_socket.sendto(msg.encode(), neighbour_address[neighbour])
                            print(f"Send to {neighbour_address[neighbour]} : {msg}")
                            found_valid_route = True
                    if not found_valid_route:
                    # Handle dead end by deleting the message
                        pass 


                # This will back track the returning message to back to the sender
                elif(parts[0] == "R" and parts[1] != station_name):
                    journey = parts[5].split('@')
                    back_station = journey[0]
                    del journey[0] 
                    journey = list_to_string(journey)
                    if back_station in neighbour_address:
                        msg = f"R~{parts[1]}~{parts[2]}~{parts[3]}~{parts[4]}~{journey}"
                        udp_socket.sendto(msg.encode(), neighbour_address[back_station])
                        print(f"Send to {neighbour_address[back_station]} : {msg}")
                    else:
                        pass
                    
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
    