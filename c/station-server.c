#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

//                cc -std=c11 -Wall -Werror -o station-server station-server.c
//                  ./assignports.sh adjacency station-server.sh 
//                  ./makewebpage.sh sstation-server.sh mywebpage.html 
//                  ./station-server.sh

void malloc_error() {
    perror("Memory not allocated"); 
    exit(EXIT_FAILURE); 
}

void parse_destination(char* query, char** destination_address, char** afterTime_address) 
{
    //split the query by the lines
    char* line = strtok(query, "\n");

    //search each line unutil destination is found
    while(line != NULL) 
    {
        //if the line starts with GET then it contains argument destination
        if(strncmp(line, "GET", 3) == 0) 
        {

            //split at space and get second word (as first word is GET)
            char* path = strtok(line, " ");
            path = strtok(NULL, " ");

            //check if path contains the destination
            if(strncmp(path, "/?to=", 5) == 0) 
            {
                //split at ?to= and get third component (first is / and second is to)
                char* destination = strtok(path, "?=&");
                destination = strtok(NULL, "?=&");
                destination = strtok(NULL, "?=&");

                //remove url parameters
                //TODO save the url param as the afterTime
                char* time = strtok(NULL, "?=&");
                time = strtok(NULL, "?=&");

                char final_time[6];
                final_time[0] = time[0];
                final_time[1] = time[1];
                final_time[2] = ':';
                final_time[3] = time[5];
                final_time[4] = time[6];
                final_time[5] = '\0';
                
                //save the destination to address if found
                printf("Destination: %s, Leaving after: %s\n", destination, final_time);
                strcpy(*(destination_address), destination);
                strcpy(*(afterTime_address), final_time);
            }
            else {
                strcpy(*(destination_address), "No Destination");
            }
        }

        //iterate through to next line
        line = strtok(NULL, "\n");
    }

}

#define MAX_LINE_LENGTH 256

//structure storing each entry of the timetable
typedef struct 
{
    //storing destination as string
    char *destination;

    //storing array of strings with departure info
    char **departureInfo;

} timetableEntry;

//timetable structure which stores all of the entries
typedef struct {
    //size of array
    int count;

    //location
    char *longitude;
    char *latitude;

    //all entries of timetable as a dynamic array of strings
    timetableEntry *timetableEntry;
}Timetable;

typedef struct {
    //name of station
    char *name;

    //ip address
    char *address;

    //port
    int port;
}Station;

//method to get minutes from %H:%M string format
int getMins(const char* timeString) 
{
    //scan string and add hour and minute to appropriate variables
    int hour, minutes;
    if (sscanf(timeString, "%d:%d", &hour, &minutes) != 2) 
    {
        //if format doesnt match, then return NULL
        return -1;
    }

    //convert hours to minutes
    int hourToMins = hour * 60;
    
    //return total time in minutes
    return (hourToMins + minutes);
}

//read the file and create the timetable
Timetable read_timetable(const char* filename) 
{
    //open the file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("file couldnt be opened");
        exit(EXIT_FAILURE);
    }

    //allocate memory for timetable and timetable array
    Timetable timetable;
    timetable.timetableEntry = calloc(1,sizeof(timetableEntry *));
    if (timetable.timetableEntry == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }

    //initialise count
    timetable.count = 0;

    //iterate through each line of the file
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) 
    {
        //skip lines starting with #
        if (line[0] != '#') 
        {
            //create new empty entry
            timetableEntry entry;

            //get first part of string
            char *departureInfoParts = strtok(line, ",");

            //first entry should be time. If the format isnt that of time, then it contains the station info
            if(getMins(departureInfoParts) == -1)
            {
                //initialise station components
                departureInfoParts = strtok(NULL, ",\n");
                timetable.longitude = malloc(strlen(departureInfoParts) + 1);
                if (timetable.longitude == NULL) {malloc_error();}
                strcpy(timetable.longitude,departureInfoParts);

                departureInfoParts = strtok(NULL, ",\n");
                timetable.latitude = malloc(strlen(departureInfoParts) + 1);
                if (timetable.latitude == NULL) {malloc_error();}
                strcpy(timetable.latitude,departureInfoParts);

                //skip to next line
                continue;
            }

            int count = 0;

            //create array to store departure info
            char **departureInfo = malloc(4*sizeof(char *));
            if (departureInfo == NULL) {malloc_error();}
            departureInfo[0] = departureInfoParts;

            //keep splitting string until you cant
            while(departureInfoParts != NULL)
            {
                //move on to next split string
                departureInfoParts = strtok(NULL, ",\n");

                //dont include last departureInfoPart as it is the destination
                if(count < 4)
                {
                    departureInfo[count + 1] = departureInfoParts;
                }
                count += 1;

                //last component is destination so copy it in
                if(count == 4)
                {
                    //allocate memory and copy destination into entry
                    entry.destination = (char *)malloc(strlen(departureInfoParts) + 1);
                    if (entry.destination == NULL) {malloc_error();}
                    strcpy(entry.destination,departureInfoParts);
                }
            }

            //store departure info into appropriate array
            entry.departureInfo = malloc(4 * sizeof(char *));
            if (entry.departureInfo == NULL) {malloc_error();}
            
            for(int j = 0; j < 4; j++)
            {
                entry.departureInfo[j] = malloc(strlen(departureInfo[j]) + 1);
                if (entry.departureInfo[j] == NULL) {malloc_error();}
                strcpy(entry.departureInfo[j],departureInfo[j]);
            }

            //add entry to timetable and resize the array
            timetable.timetableEntry = realloc(timetable.timetableEntry, (timetable.count + 1) * sizeof(timetableEntry));
            if (timetable.timetableEntry == NULL) {malloc_error();}

            timetable.timetableEntry[timetable.count] = entry;
            timetable.count++;
        }
    }

    //close file and return the timetable
    fclose(file);
    return timetable;
}

Timetable filter_timetable(Timetable timetable, char* cutOffTimeStr) 
{
    //convert the cutt off timeto mins
    int cutOffTime = getMins(cutOffTimeStr);

    //allocate memory for timetable and timetable array
    Timetable filteredTimetable;
    filteredTimetable.timetableEntry = calloc(1,sizeof(timetableEntry *));
    if (filteredTimetable.timetableEntry == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }

    //initialise station components
    filteredTimetable.count = 0;

    filteredTimetable.longitude = malloc(strlen(timetable.longitude) + 1);
    if (filteredTimetable.longitude == NULL) {malloc_error();}
    strcpy(filteredTimetable.longitude,timetable.longitude);

    filteredTimetable.latitude = malloc(strlen(timetable.latitude) + 1);
    if (filteredTimetable.latitude == NULL) {malloc_error();}
    strcpy(filteredTimetable.latitude,timetable.latitude);


    //iterate through each entry in timetable
    for(int i = 0; i < timetable.count; i++)
    {
        timetableEntry entry = timetable.timetableEntry[i];

        //first element of departure info is departure time
        char *departureTimeStr = entry.departureInfo[0];
        int departureTime = getMins(departureTimeStr);

        //Compare the departure time with the cutoff time
        if (departureTime >= cutOffTime)
        {
            //create new empty entry
            timetableEntry filteredEntry;
            
            //copy filtered components into new empty entry
            filteredEntry.destination = malloc(strlen(entry.destination) + 1);
            if (filteredEntry.destination == NULL) {malloc_error();}
            strcpy(filteredEntry.destination,entry.destination);

            filteredEntry.departureInfo = malloc(4*sizeof(char *)); 
            if (filteredEntry.departureInfo == NULL) {malloc_error();}

            for(int j = 0; j < 4; j++)
            {
                filteredEntry.departureInfo[j] = malloc(strlen(entry.departureInfo[j]) + 1);
                if (filteredEntry.departureInfo[j] == NULL) {malloc_error();}
                strcpy(filteredEntry.departureInfo[j],entry.departureInfo[j]);
            }

            //add entry to filtered timetable and resize the array
            filteredTimetable.timetableEntry = realloc(filteredTimetable.timetableEntry, (filteredTimetable.count + 1) * sizeof(timetableEntry));
            if (filteredTimetable.timetableEntry == NULL) {malloc_error();}
            filteredTimetable.timetableEntry[filteredTimetable.count] = filteredEntry;
            filteredTimetable.count++;
        }
    }

    return filteredTimetable;
}

char *generate_http_response(int statusCode, char* responseBody) 
{
    char *response;

    // OK response
    if (statusCode == 200) 
    {
        //store length of response body as string
        char *responseBodyLength = malloc(50);
        if (responseBodyLength == NULL) {malloc_error();}
        sprintf(responseBodyLength,"%ld",strlen(responseBody));

        //allocate memory for response and write message to it
        response = malloc(strlen("HTTP/1.1 200 OK\r\nContent-Length: %s\r\n\r\n%s") + strlen(responseBody) + strlen(responseBodyLength));
        if (response == NULL) {malloc_error();}
        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %s\r\n\r\n%s", responseBodyLength, responseBody);
    } 
    // Not Found response
    else if (statusCode == 404) 
    {
        response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    } 
    // Invalid status code
    else 
    {
        //store statusCode as string
        char *statusCodeStr = malloc(3);
        if (statusCodeStr == NULL) {malloc_error();}
        sprintf(statusCodeStr,"%d",statusCode);

        //allocate memory for response and write message to it
        response = malloc(strlen("HTTP/1.1 %d\r\nContent-Length: 0\r\n\r\n") + strlen(statusCodeStr));
        if (response == NULL) {malloc_error();}
        sprintf(response, "HTTP/1.1 %d\r\nContent-Length: 0\r\n\r\n", statusCode);
    }

    return response;
}

//filter the timetable by the destination
Timetable destination_timetable(Timetable timetable, char *destination)
{
    //create and allocate memory for timetable containing entries leading only to destination
    Timetable allDestinationTimetable;
    allDestinationTimetable.timetableEntry = calloc(1,sizeof(timetableEntry));
    if (allDestinationTimetable.timetableEntry == NULL) {malloc_error();}

    //initialise station components
    allDestinationTimetable.count = 0;

    allDestinationTimetable.longitude = malloc(strlen(timetable.longitude) + 1);
    if (allDestinationTimetable.longitude == NULL) {malloc_error();}
    strcpy(allDestinationTimetable.longitude,timetable.longitude);

    allDestinationTimetable.latitude = malloc(strlen(timetable.latitude) + 1);
    if (allDestinationTimetable.latitude == NULL) {malloc_error();}
    strcpy(allDestinationTimetable.latitude,timetable.latitude);

    //add all entries which lead to the required destination to newly created timetable
    for(int i = 0; i < timetable.count; i++)
    {
        timetableEntry entry = timetable.timetableEntry[i];

        if(strcmp(destination,entry.destination) == 0)
        {
            //add entry to timetable and resize the array
            allDestinationTimetable.timetableEntry = realloc(allDestinationTimetable.timetableEntry, (allDestinationTimetable.count + 1) * sizeof(timetableEntry));
            if (allDestinationTimetable.timetableEntry == NULL) {malloc_error();}
            allDestinationTimetable.timetableEntry[allDestinationTimetable.count] = entry;
            allDestinationTimetable.count++;
        }
    }

    return allDestinationTimetable;
}

//find the fastest route from a timetable of routes (provided it is after the aftertime)
char *find_fastest_route(Timetable allDestinationTimetable, char *after_time_str) 
{
    //turn after time into mins
    int afterTimeMins = getMins(after_time_str);

    //initial conditions
    char* fastestRoute = NULL;
    double fastestDuration = 9999999999999999999.9;

    //iterate through every destination timetable route
    for(int i = 0; i < allDestinationTimetable.count; i++)
    {
        timetableEntry entry = allDestinationTimetable.timetableEntry[i];

        //convert to mins
        int arrivalTimeMins = getMins(entry.departureInfo[3]);

        //calculate duration
        double duration = arrivalTimeMins - afterTimeMins;

        //find the fastest route
        if (duration < fastestDuration) 
        {
            char *route = malloc(strlen("('%s', '%s', '%s', '%s')") + strlen(entry.departureInfo[0]) + strlen(entry.departureInfo[1])+ strlen(entry.departureInfo[2])+ strlen(entry.departureInfo[3]));
            sprintf(route,"('%s', '%s', '%s', '%s')",entry.departureInfo[0],entry.departureInfo[1],entry.departureInfo[2],entry.departureInfo[3]);
            fastestRoute = malloc(strlen(route) + 1);
            if (fastestRoute == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }
            strcpy(fastestRoute,route);

            fastestDuration = duration;
        }
    }

    return fastestRoute;
}

//declare UDP socket earlier for this function
int udpServerSocket;

//method to send a single UDP message
void send_a_udp_message(char* message, char* address, int port) {
    struct sockaddr_in destination;
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = inet_addr(address);
    address = strtok(NULL, ":");
    destination.sin_port = htons(port);

    sendto(udpServerSocket, message, strlen(message), 0, (const struct sockaddr*)&destination, sizeof(destination));
}

//neighbors_len and neighbors_dict declared here for this function
int neighbors_len;
Station* neighbors_dict;

//converts an ip address into a station object, based off the servers neighbor dict
Station* ip_to_station(char* neighbor) {
    Station* return_station;
    return_station = (Station*) malloc(sizeof(Station));
    if (return_station == NULL) {malloc_error();}

    //slice ip address to just port
    char* address = strtok(neighbor, ":");
    address = strtok(NULL, ":");
    int port = atoi(address);

    //search for matching port in neighbors_dict
    for (int i = 0; i < neighbors_len; i++) {
        if (neighbors_dict[i].port == port) {
            *(return_station) = neighbors_dict[i];
        }
    }

    return return_station;
}

//converts a station name into a station object, based off the servers neighbor dict
Station* name_to_station(char* neighbor) {
    Station* return_station;
    return_station = (Station*) malloc(sizeof(Station));
    if (return_station == NULL) {malloc_error();}

    //search for matching name in neighbors_dict
    for (int i = 0; i < neighbors_len; i++) {
        if (strcmp(neighbors_dict[i].name, neighbor) == 0) {
            *(return_station) = neighbors_dict[i];
        }
    }

    return return_station;
}

//received_len and received_dict declared here for this function
int received_len;
char** received_dict;

//globally defined for replies later
char* reply_destination;

//once all the R messages have returned, searches them for the fastest one, saves that as the route
char* choose_fastest_route() {
    if (received_len == 0) {
        return "No valid route";
    }

    char* best_route;
    char* fastest_time = "23:59";

    for (int i = 0; i < received_len; i++) {
        char* arrive_time = malloc(strlen(received_dict[i]) + 1);
        if (arrive_time == NULL) {malloc_error();}
        //slicing from -7 to -2
        strncpy(arrive_time, received_dict[i] + strlen(received_dict[i])-7, 5);

        if (strcmp(arrive_time, fastest_time) < 0) { //negative value if arrive less than fastest
            fastest_time = arrive_time; 
            best_route = received_dict[i];
        }
    }

    //replace \@s with line breaks
    for (int i = 0; i < strlen(best_route); i++) {
        if (best_route[i] == '@') {
            best_route[i] = '\n';
        }
    }

    return best_route;
}

#define MAX_BUFFER_SIZE 1024

void start_server(char* stationName, int browser_port, int query_port, char** neighbors, int num_neighbors) 
{

    //read file and get timetable
    char *filename = malloc(strlen("tt-%s") + strlen(stationName));
    if (filename == NULL) {malloc_error();}
    sprintf(filename,"tt-%s",stationName);
    Timetable stationTimetable = read_timetable(filename);

    //store initial mtime of file
    struct stat filestat;
    stat(filename, &filestat);
    __time_t last_mtime = filestat.st_mtime;
    
    //declare variables required for tcp and udp
    int tcpServerSocket, newSocket; //udpsocket declared earlier
    struct sockaddr_in tcp_server_addr, udp_server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int numbytes;
    char udpDatagram[MAX_BUFFER_SIZE];

    //message id, increments for each message sent, used to identify duplicates
    int message_id = -1;

    // Create a TCP socket for handling queries from the web interface
    if ((tcpServerSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Create a UDP socket for handling queries from stations
    if ((udpServerSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the tcp socket
    memset(&tcp_server_addr, 0, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_server_addr.sin_port = htons(browser_port);

    if (bind(tcpServerSocket, (struct sockaddr*)&tcp_server_addr, sizeof(tcp_server_addr)) < 0) 
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Bind the udp socket
    memset(&udp_server_addr, 0, sizeof(udp_server_addr));
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_server_addr.sin_port = htons(query_port);

    if (bind(udpServerSocket, (const struct sockaddr *)&udp_server_addr, sizeof(udp_server_addr)) < 0) 
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(tcpServerSocket, 5) < 0) 
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started for %s on browser port %d and query port %d\n", stationName, browser_port, query_port);

    //wait a second for all other servers to start before initialising network
    sleep(1);
    
    //initialise array for neighboring stations (declared earlier)
    neighbors_len = 0;
    neighbors_dict = malloc(neighbors_len * sizeof(Station));

    char* i_message = malloc(3 + strlen(stationName));
    if (i_message == NULL) {malloc_error();}
    sprintf(i_message, "I~%s", stationName);

    //send identifying message to all neighbors
    printf("Sending identification messages from %s\n", stationName);
    for (int i = 0; i < num_neighbors; i++) {
        //copying neighbors[i] into a new string so that strtok doesn't mutate the original
        char* neighbor = malloc(strlen(neighbors[i]) + 1); 
        if (neighbor == NULL) {malloc_error();}
        strcpy(neighbor, neighbors[i]);

        //slicing address and sending udp message
        char* address = strtok(neighbor, ":");
        int port = atoi(strtok(NULL, ":"));
        send_a_udp_message(i_message, address, port);
        printf("    %s: Sent %s to %s\n", stationName, i_message, neighbors[i]);
    }

    //create array for requests that have visited before
    int visited_len = 0;
    char** visited_dict = malloc(visited_len * sizeof(char*));
    if(visited_dict == NULL) {malloc_error();}

    //create array to store received routes in
    received_len = 0;
    received_dict = malloc(received_len * sizeof(char*));
    if(received_dict == NULL) {malloc_error();}

    //used for select
    fd_set readset;
    int maxFileDescriptor;

    //used for a timer
    time_t query_got = 99999999999999999;
    time_t timer;

    while(1) 
    {
        //check query timeout clock
        time(&timer);
        if (timer > query_got) {
            query_got = 99999999999999999;

            printf("    %s: Picking fastest route out of %i response(s)\n", stationName, received_len);
            //pick the fastest route out of all the possibilities
            char* route = choose_fastest_route();

            // Format the response message with the timetable information
            char *responseBody = malloc(strlen("Fastest route to %s:\n%s") + strlen(route) + strlen(reply_destination));
            if(responseBody == NULL) {malloc_error();}
            sprintf(responseBody,"Fastest route to %s:\n%s", reply_destination, route);

            // Format the HTTP response
            char* response = generate_http_response(200, responseBody);

            // Send the response back to the web interface
            write(newSocket, response, strlen(response));

            // Clean up the connection
            printf("%s\n", responseBody);
            printf("Closed after finding route\n");
            close(newSocket);
            //"clear" received dict
            received_len = 0;
        }

        //restat the timetable file
        stat(filename, &filestat);
        //if its been modified, reread the timetable, filter it, and update last_mtime
        if(filestat.st_mtime != last_mtime) 
        {
            stationTimetable = read_timetable(filename);
            last_mtime = filestat.st_mtime;
            printf("Updated timetable info for %s\n", stationName);
        }

        //add to readset
        FD_CLR(tcpServerSocket, &readset);
        FD_CLR(udpServerSocket, &readset);
        FD_SET(tcpServerSocket, &readset);
        FD_SET(udpServerSocket, &readset);

        //find the max
        if(tcpServerSocket > udpServerSocket)
        {
            maxFileDescriptor = tcpServerSocket;
        }else
        {
            maxFileDescriptor = udpServerSocket;
        }

        //declare and initialise the timeout value (currently 3 seconds)
        struct timeval timeout;
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        //try and select a server socket (either udp or tcp)
        if(select(maxFileDescriptor + 1, &readset, NULL, NULL, &timeout) >= 0) 
        {
            //check if it is a tcp connection
            if (FD_ISSET(tcpServerSocket, &readset)) 
            {
                // Accept a new connection
                if((newSocket = accept(tcpServerSocket, (struct sockaddr*)&client_addr, &addr_len)) < 0) 
                {
                    perror("Accept failed");
                    exit(EXIT_FAILURE);
                }
                printf("\nConnection from %s:%i to %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), stationName);
                
                // Receive the query from the web interface
                char query[MAX_BUFFER_SIZE];
                memset(query, 0, sizeof(query));
                read(newSocket, query, sizeof(query));
                //cut TCP query down to just the first line
                char* cut_query = strtok(query, "\n");
                printf("    %s: Received TCP, %s\n", stationName, cut_query);
                
                //parse the destination and after time
                char *destination = malloc(100);
                char *afterTime = malloc(6);
                if (destination == NULL || afterTime == NULL) {malloc_error();}
                parse_destination(query, &destination, &afterTime);
                if(strcmp(destination, "No Destination") == 0)
                {
                    // Clean up the connection
                    printf("Closed due to null destination\n");
                    close(newSocket);
                    continue;
                }

                //get the fastest route
                Timetable filteredTimetable = filter_timetable(stationTimetable, afterTime);
                Timetable destinationTimetable = destination_timetable(filteredTimetable, destination);
                char* route = find_fastest_route(destinationTimetable, afterTime);
                
                //check if source neighbors destination
                bool neighbors_destination = false;
                //iterate through the neighbors dict
                for (int i = 0; i < neighbors_len; i++) {
                    //if queried destination is in the dict
                    if (strcmp(destination, neighbors_dict[i].name) == 0) {
                        neighbors_destination = true;
                        break;
                    }
                }

                //if station does not neighbor the destination, send out UDP request
                if (!neighbors_destination) {
                    //start the timeout for reporting no route 
                    time(&query_got);
                    message_id++;
                    //construct the initial M message, send it to all neighbors
                    //M~source_station_name~destination_station_name~time~route~journey
                    for (int i = 0; i < num_neighbors; i++) {
                        //copying neighbors[i] into a new string so that strtok doesn't mutate the original
                        char* neighbor = malloc(strlen(neighbors[i]) + 1); 
                        if (neighbor == NULL) {malloc_error();}
                        strcpy(neighbor, neighbors[i]);

                        //getting a station object for the neighbor
                        Station* neighbor_station = ip_to_station(neighbor);

                        //find fastest route to neighbor
                        Timetable destinationTimetable = destination_timetable(filteredTimetable, neighbor_station->name);
                        char* neighbor_route = find_fastest_route(destinationTimetable, afterTime);

                        if (neighbor_route == NULL) {
                            printf("    %s: No valid route\n", stationName);
                            continue;
                        }

                        //extract the last arrival time from route
                        char* new_afterTime = malloc(strlen(neighbor_route) + 1);
                        if (new_afterTime == NULL) {malloc_error();}
                        //slicing from -7 to -2
                        strncpy(new_afterTime, neighbor_route + strlen(neighbor_route)-7, 5);

                        //construct initial m message
                        char* m_message = malloc(8 + strlen(stationName)*2 + 1 + strlen(destination) + 5 + strlen(neighbor_route));
                        if (m_message == NULL) {malloc_error();}
                        sprintf(m_message, "M~%s~%i~%s~%s~%s~%s", stationName, message_id, destination, new_afterTime, neighbor_route, stationName);
                        
                        //send message
                        send_a_udp_message(m_message, neighbor_station->address, neighbor_station->port);
                        printf("    %s: Sent %s to %s\n", stationName, m_message, neighbor_station->name);

                        //add this message's id to the dict of messages its seen
                        char* source_id = malloc(strlen(stationName) + 1 + 2);
                        if (source_id == NULL) {malloc_error();}
                        sprintf(source_id, "%s~%i", stationName, message_id);
                        
                        //add the message about to be sent to the visited dict
                        visited_dict = realloc(visited_dict, (visited_len + 1) * sizeof(char*));
                        if (visited_dict == NULL) {malloc_error();}
                        visited_dict[visited_len] = malloc(strlen(source_id) + 1);
                        if (visited_dict[visited_len] == NULL) {malloc_error();}
                        strcpy(visited_dict[visited_len], source_id);
                        visited_len++;
                    }

                    //dummy status so that it doesn't close socket or write yet
                    route = "Searching...";

                    //save the destination for the response
                    reply_destination = destination;

                }

                if(route == NULL)
                {
                    route = malloc(strlen("There is no journey from %s to %s leaving after %s today") + strlen(stationName) + strlen(destination) + strlen(afterTime));
                    if (route == NULL) {malloc_error();}
                    sprintf(route,"There is no journey from %s to %s leaving after %s today",stationName,destination,afterTime);
                }

                if(strcmp(route, "Searching...") != 0) {
                    // Format the response message with the timetable information
                    char *responseBody = malloc(strlen("Fastest route to :%s\n%s") + strlen(route) + strlen(destination));
                    if(responseBody == NULL) {malloc_error();}
                    sprintf(responseBody,"Fastest route to %s:\n%s",destination,route);

                    // Format the HTTP response
                    char* response = generate_http_response(200, responseBody);

                    // Send the response back to the web interface
                    write(newSocket, response, strlen(response));
                    

                    // Clean up the connection
                    printf("    %s: Sent response %s\n", stationName, responseBody);
                    printf("Closed after finding route\n");
                    close(newSocket);
                }

            }



            //check if it is an udp connection
            if(FD_ISSET(udpServerSocket, &readset))
            {
                // Receive data from the UDP socket
                numbytes = recvfrom(udpServerSocket, udpDatagram, MAX_BUFFER_SIZE - 1, 0, (struct sockaddr *) &client_addr, &addr_len);
                if (numbytes < 0) {
                    perror("Failed to receive UDP datagram");
                    continue;
                }

                // turn datagram into string
                udpDatagram[numbytes] = '\0';
                printf("    %s: Received %s from %s:%i\n", stationName, udpDatagram, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                //get components from datagram
                char *datagramParts = strtok(udpDatagram, "~");
                char *messageType = malloc(strlen(datagramParts) + 1);
                if (messageType == NULL) {malloc_error();}
                strcpy(messageType, datagramParts);

                //M, for message, message sent from source to destination
                //format: M~source_station_name~destination_station_name~time~route~journey
                //route is the list of bus/train routes to deliver to the user
                //journey is the list of station servers visited by the packet
                if(strcmp(messageType, "M") == 0)
                {
                    //source station name
                    datagramParts = strtok(NULL, "~");
                    char *sourceStation = malloc(strlen(datagramParts) + 1);
                    if (sourceStation == NULL) {malloc_error();}
                    strcpy(sourceStation,datagramParts);

                    //message id
                    datagramParts = strtok(NULL, "~");
                    char *id = malloc(strlen(datagramParts) + 1);
                    if (id == NULL) {malloc_error();}
                    strcpy(id, datagramParts);

                    //combine source and message id
                    char* source_id = malloc(strlen(sourceStation) + strlen(id) + 2);
                    if (source_id == NULL) {malloc_error();}
                    sprintf(source_id, "%s~%s", sourceStation, id);

                    //if the request has visited here previously
                    bool drop_packet = false;
                    for (int j = 0; j < visited_len; j++){
                        if(strcmp(visited_dict[j], source_id) == 0) {
                            //DROP PACKET
                            printf("    %s: Dropped %s (already seen)\n", stationName, source_id);
                            drop_packet = true;
                        }
                    }
                    if(drop_packet) {drop_packet = false; continue;}

                    //destination station name
                    datagramParts = strtok(NULL, "~");
                    char *destStation = malloc(strlen(datagramParts) + 1);
                    if (destStation == NULL) {malloc_error();}
                    strcpy(destStation, datagramParts);

                    //if the destination is reached
                    if(strcmp(destStation, stationName) == 0)
                    {
                        printf("    %s: %s has reached its destination!\n", stationName, source_id);

                        //aftertime
                        datagramParts = strtok(NULL, "~");

                        //route
                        datagramParts = strtok(NULL, "~");
                        char *route = malloc(strlen(datagramParts) + 1);
                        if (route == NULL) {malloc_error();}
                        strcpy(route, datagramParts);

                        //journey
                        datagramParts = strtok(NULL, "~");
                        char *journey = malloc(strlen(datagramParts) + strlen(stationName) + 2);
                        if (journey == NULL) {malloc_error();}
                        strcpy(journey, datagramParts);

                        //get most recent stop on the journey
                        char* last_stop = strtok(datagramParts, "@");
                        journey += strlen(last_stop) + 1; //increment pointer to cut off the first station

                        //construct the r message
                        char* r_message = malloc(5 + strlen(source_id) + strlen(route) + strlen(journey));
                        if (r_message == NULL) {malloc_error();}
                        sprintf(r_message, "R~%s~%s~%s~%s", source_id, destStation, route, journey);

                        //send the r message to the most recent stop on journey
                        Station* last_station = name_to_station(last_stop);
                        send_a_udp_message(r_message, last_station->address, last_station->port);
                        printf("    %s: Sent %s to %s\n", stationName, r_message, last_station->name);

                        //dont relay m message any further
                        continue;
                    }

                    //aftertime
                    datagramParts = strtok(NULL, "~");
                    char *currentTime = malloc(strlen(datagramParts) + 1);
                    if (currentTime == NULL) {malloc_error();}
                    strcpy(currentTime, datagramParts);

                    //route
                    datagramParts = strtok(NULL, "~");
                    char *routeSoFar = malloc(strlen(datagramParts) + 1);
                    if (routeSoFar == NULL) {malloc_error();}
                    strcpy(routeSoFar, datagramParts);

                    //journey
                    datagramParts = strtok(NULL, "~");
                    char *journeySoFar = malloc(strlen(datagramParts) + strlen(stationName) + 2);
                    if (journeySoFar == NULL) {malloc_error();}
                    sprintf(journeySoFar, "%s@%s", stationName, datagramParts);

                    //now that the full message is read, add source_id to visited dict                 
                    visited_dict = realloc(visited_dict, (visited_len + 1) * sizeof(char*));
                    if (visited_dict == NULL) {malloc_error();}
                    visited_dict[visited_len] = malloc(strlen(source_id) + 1);
                    if (visited_dict[visited_len] == NULL) {malloc_error();}
                    strcpy(visited_dict[visited_len], source_id);
                    visited_len++;

                    //for each neighbour of this node
                    for (int i = 0; i < num_neighbors; i++) {
                        //copying neighbors[i] into a new string so that strtok doesn't mutate the original
                        char* neighbor = malloc(strlen(neighbors[i]) + 1); 
                        if (neighbor == NULL) {malloc_error();}
                        strcpy(neighbor, neighbors[i]);

                        //getting a station object for the neighbor
                        Station* neighbor_station = ip_to_station(neighbor);

                        //find fastest route to neighbor
                        Timetable newStationTimetable = read_timetable(filename);
                        Timetable newFilteredTimetable = filter_timetable(newStationTimetable, currentTime);
                        Timetable newDestinationTimetable = destination_timetable(newFilteredTimetable, neighbor_station->name);
                        char* route = find_fastest_route(newDestinationTimetable, currentTime);

                        if (route == NULL) {
                            printf("    %s: No valid route\n", stationName);
                            continue;
                        }

                        //extract the last arrival time from route
                        char* new_afterTime = malloc(strlen(route) + 1);
                        if (new_afterTime == NULL) {malloc_error();}
                        //slicing from -7 to -2
                        strncpy(new_afterTime, route + strlen(route)-7, 5);

                        //add route to a copy of routeSoFar
                        char* new_routeSoFar = malloc(strlen(routeSoFar) + strlen(route) + 2);
                        if (new_routeSoFar == NULL) {malloc_error();}
                        sprintf(new_routeSoFar, "%s@%s", routeSoFar, route);

                        //construct new m message
                        char* m_message = malloc(8 + strlen(sourceStation) + strlen(id) + strlen(destStation) + 5 + strlen(new_routeSoFar) + strlen(journeySoFar));
                        if (m_message == NULL) {malloc_error();}
                        sprintf(m_message, "M~%s~%s~%s~%s~%s~%s", sourceStation, id, destStation, new_afterTime, new_routeSoFar, journeySoFar);
                        
                        //send message
                        send_a_udp_message(m_message, neighbor_station->address, neighbor_station->port);
                        printf("    %s: Sent %s to %s\n", stationName, m_message, neighbor_station->name);
                    }
                }

                //A, for acknowledgements
                //format: A
                if(strcmp(messageType,"A") == 0)
                {
                    //TODO
                }

                //R, for response, message sent from destination back to source
                //format: R~route~journey
                //route is the list of bus/train routes to deliver to the user
                //journey is the list of station servers visited by the packet
                //packet will reuse journey in reverse to trace steps back
                if(strcmp(messageType,"R") == 0)
                {
                    //source_id
                    datagramParts = strtok(NULL, "~");
                    char *source = malloc(strlen(datagramParts) + 1);
                    if (source == NULL) {malloc_error();}
                    strcpy(source, datagramParts);

                    datagramParts = strtok(NULL, "~");
                    char *id = malloc(strlen(datagramParts) + 1);
                    if (id == NULL) {malloc_error();}
                    strcpy(id, datagramParts);
                    
                    char* source_id = malloc(strlen(source) + strlen(id) + 1);
                    if (source_id == NULL) {malloc_error();}
                    sprintf(source_id, "%s~%s", source, id);

                    //destination station name
                    datagramParts = strtok(NULL, "~");
                    char *destStation = malloc(strlen(datagramParts) + 1);
                    if (destStation == NULL) {malloc_error();}
                    strcpy(destStation, datagramParts);

                    //route
                    datagramParts = strtok(NULL, "~");
                    char *route = malloc(strlen(datagramParts) + 1);
                    if (route == NULL) {malloc_error();}
                    strcpy(route, datagramParts);

                    //journey
                    datagramParts = strtok(NULL, "~");
                    if (datagramParts == NULL) { // if journey as null, has reached destination
                        printf("    %s: Reply has reached its source!\n", stationName);

                        //add R messages route to the received dict
                        received_dict = realloc(received_dict, (received_len + 1) * sizeof(char*));
                        if (received_dict == NULL) {malloc_error();}
                        received_dict[received_len] = malloc(strlen(route) + 1);
                        if (received_dict[received_len] == NULL) {malloc_error();}
                        strcpy(received_dict[received_len], route);
                        received_len++;

                        printf("    %s: Added to received_dict %s\n", stationName, route);
                        continue;
                    }

                    char* journey = malloc(strlen(datagramParts));
                    if (journey == NULL) {malloc_error();}
                    strcpy(journey, datagramParts);

                    //get most recent stop on the journey
                    char* last_stop = strtok(datagramParts, "@");
                    printf("last stop %s\n", last_stop);
                    journey += (strlen(last_stop)); //increment pointer to cut off the first station
                    char* next_stop = strtok(NULL, "@");
                    if(next_stop != NULL) {journey++;} //extra increment for the @

                    //construct the r message
                    char* r_message = malloc(3 + strlen(route) + strlen(journey));
                    if (r_message == NULL) {malloc_error();}
                    sprintf(r_message, "R~%s~%s~%s~%s", source_id, destStation, route, journey);

                    //send the r message to the most recent stop on journey
                    printf("%s\n", last_stop);
                    Station* last_station = name_to_station(last_stop);
                    send_a_udp_message(r_message, last_station->address, last_station->port);
                    printf("    %s: Sent %s to %s\n", stationName, r_message, last_station->name);
                }

                //I, for initialise, sends name, IP and address to neighboring servers on startup
                //format: I~name~address~port
                if(strcmp(messageType, "I") == 0)
                {
                    //expand neighbors dict array
                    neighbors_dict = realloc(neighbors_dict, (neighbors_len + 1) * sizeof(Station));
                    if (neighbors_dict == NULL) {malloc_error();}

                    //get name
                    datagramParts = strtok(NULL,"~");
                    neighbors_dict[neighbors_len].name = malloc(strlen(datagramParts) + 1);
                    if (neighbors_dict[neighbors_len].name == NULL) {malloc_error();}
                    strcpy(neighbors_dict[neighbors_len].name, datagramParts);

                    //add address and port to dict
                    neighbors_dict[neighbors_len].address = malloc(strlen(datagramParts) + 1);
                    if (neighbors_dict[neighbors_len].address == NULL) {malloc_error();}
                    strcpy(neighbors_dict[neighbors_len].address, inet_ntoa(client_addr.sin_addr));

                    neighbors_dict[neighbors_len].port = ntohs(client_addr.sin_port);

                    printf("    %s: Added to neighbors_dict %s = %s:%i\n", stationName, neighbors_dict[neighbors_len].name, 
                    neighbors_dict[neighbors_len].address, neighbors_dict[neighbors_len].port);

                    //increment length of array
                    neighbors_len++;
                }

            }
           
        }

    }

    // Close the server sockets
    close(tcpServerSocket);
    close(udpServerSocket);
}

int main(int argc, char* argv[]) 
{
    // Check if the correct number of command line arguments are provided
    if (argc < 5) 
    {
        printf("Usage: ./station-server station-name browser-port query-port neighbor1 [neighbor2 ...]\n");
        exit(EXIT_FAILURE);
    }

    // Extract command line arguments
    char* station_name = argv[1];
    int browser_port = atoi(argv[2]);
    int query_port = atoi(argv[3]);
    char** neighbors = malloc(sizeof(char *));

    //can have many neighbors so add all command line arguments after 3 into neighbors
    if(neighbors == NULL) {malloc_error();}
    int count = 0;
    for(int i = 4; i < argc; i++)
    {
        neighbors[count] = argv[i];
        neighbors = realloc(neighbors, (count + 1) * sizeof(char *) + 1);
        if(neighbors == NULL) {malloc_error();}
        count++;
    }

    // Start the server with the provided configuration
    start_server(station_name, browser_port, query_port, neighbors, count);

    printf("%s has closed\n", station_name);
    return 0;
}

//sliding window
//acknowledgements

//update current time from url parameter (super optional)

//add message id's to R messages
//send a special R message back if too late
//real dataset
