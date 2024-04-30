#!/usr/bin/python3


import sys
import socket
import datetime


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

def filter_timetable(timetable, cutoff_time_str):
    # Parse the cutoff time string into a datetime object
    cutoff_time = datetime.datetime.strptime(cutoff_time_str, '%H:%M')

    filtered_timetable = {}
    for destination, departures in timetable.items():
        filtered_departures = []
        for departure_info in departures:
            departure_time_str = departure_info[0]  # Assuming departure time is the first element
            # Parse the departure time string into a datetime object
            departure_time = datetime.datetime.strptime(departure_time_str, '%H:%M')
            # Compare the parsed departure time with the cutoff time
            if departure_time >= cutoff_time:
                filtered_departures.append(departure_info)
        filtered_timetable[destination] = filtered_departures
    return filtered_timetable

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

def generate_http_response(status_code, response_body):
    if status_code == 200:
        # OK response
        response = f"HTTP/1.1 200 OK\r\nContent-Length: {len(response_body)}\r\n\r\n{response_body}"
    elif status_code == 404:
        # Not Found response
        response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
    else:
        # Invalid status code
        response = f"HTTP/1.1 {status_code}\r\nContent-Length: 0\r\n\r\n"

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
        
        # Calculate the duration of the route, including waiting time
        duration = departure_time - after_time + (arrival_time - departure_time)

        # Check if this is the fastest route found so far
        if fastest_duration is None or duration < fastest_duration:
            fastest_duration = duration
            fastest_route = entry

    return fastest_route


def start_server(station_name, browser_port, query_port, neighbors):
    # Convert neighbor strings to tuples of (hostname, port)
    neighbors = [neighbor.split(":") for neighbor in neighbors]

    # Convert port numbers to integers
    browser_port = int(browser_port)
    query_port = int(query_port)

    # Create a TCP/IP socket for handling queries from the web interface
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('localhost', browser_port))
    server_socket.listen(5)

    filename = f"tt-{station_name}"
    station_timetable = read_timetable(filename)
    print(f"Server started for {station_name} on browser port {browser_port} and query port {query_port}")

    while True:
        # Wait for a connection from the web interface
        print("Waiting for a connection from the web interface...")
        connection, client_address = server_socket.accept()

        try:
            print(f"Connection from {client_address}")

            # Receive the query from the web interface
            data = connection.recv(1024)
            rev_query = data.decode()
            print(f"recived query: {rev_query}")


            # Process the query (you need to implement this part)
            destination = parse_destination(rev_query)
            after_time = '10:00'
            filtered_timetable = filter_timetable(station_timetable, after_time)
            # Get the timetable entries for the destination
            route = find_fastest_route(filtered_timetable, destination, after_time)
            # Format the response message with the timetable information
 
            response_body = f"Fastest route to {destination}:\n"
            response_body += f"{route}"

            # Format the HTTP response
            response = generate_http_response(200, response_body)
            # Send the response back to the web interface
            connection.sendall(response.encode())
        finally:
            # Clean up the connection
            print("Closed")
            connection.close()

if __name__ == "__main__":
    # Check if the correct number of command line arguments are provided
    if len(sys.argv) < 5:
        print("Usage: ./station-server.py station-name browser-port query-port neighbor1 [neighbor2 ...]")
        sys.exit(1)

    # Extract command line arguments
    station_name = sys.argv[1]
    browser_port = sys.argv[2]
    query_port = sys.argv[3]
    neighbors = sys.argv[4:]

    # Start the server with the provided configuration
    start_server(station_name, browser_port, query_port, neighbors)

