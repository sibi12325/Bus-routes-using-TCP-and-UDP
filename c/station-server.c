#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>

//                cc -std=c11 -Wall -Werror -o station-server station-server.c
//                  ./assignports.sh adjacency station-server.sh 
//                  ./makewebpage.sh sstation-server.sh mywebpage.html 
//                  ./station-server.sh

char* parse_destination(char* query) 
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
                char* destination = strtok(path, "?=");
                destination = strtok(NULL, "?=");
                destination = strtok(NULL, "?=");
                //return the destination if found
                return destination;
            }
        }

        //iterate through to next line
        line = strtok(NULL, "\n");
    }

    //if no destination was found return NULL
    return NULL;
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
                if (timetable.longitude == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(timetable.longitude,departureInfoParts);

                departureInfoParts = strtok(NULL, ",\n");
                timetable.latitude = malloc(strlen(departureInfoParts) + 1);
                if (timetable.latitude == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(timetable.latitude,departureInfoParts);

                //skip to next line
                continue;
            }

            int count = 0;

            //create array to store departure info
            char **departureInfo = malloc(4*sizeof(char *));
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
                    if (entry.destination == NULL) 
                    { 
                        perror("Memory not allocated"); 
                        exit(EXIT_FAILURE);
                    }
                    strcpy(entry.destination,departureInfoParts);
                }
            }

            //store departure info into appropriate array
            entry.departureInfo = malloc(4 * sizeof(char *));
            
            for(int j = 0; j < 4; j++)
            {
                entry.departureInfo[j] = malloc(strlen(departureInfo[j]) + 1);
                strcpy(entry.departureInfo[j],departureInfo[j]);
            }

            //add entry to timetable and resize the array
            timetable.timetableEntry = realloc(timetable.timetableEntry, (timetable.count + 1) * sizeof(timetableEntry));
            if (timetable.timetableEntry == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }

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
    if (filteredTimetable.longitude == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
    strcpy(filteredTimetable.longitude,timetable.longitude);

    filteredTimetable.latitude = malloc(strlen(timetable.latitude) + 1);
    if (filteredTimetable.latitude == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
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
            if (filteredEntry.destination == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }
            strcpy(filteredEntry.destination,entry.destination);

            filteredEntry.departureInfo = malloc(4*sizeof(char *)); 

            for(int j = 0; j < 4; j++)
            {
                filteredEntry.departureInfo[j] = malloc(strlen(entry.departureInfo[j]) + 1);
                strcpy(filteredEntry.departureInfo[j],entry.departureInfo[j]);
            }

            //add entry to filtered timetable and resize the array
            filteredTimetable.timetableEntry = realloc(filteredTimetable.timetableEntry, (filteredTimetable.count + 1) * sizeof(timetableEntry));
            if (filteredTimetable.timetableEntry == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }
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
        if (responseBodyLength == NULL) 
        { 
            perror("Memory not allocated"); 
            exit(EXIT_FAILURE);
        }
        sprintf(responseBodyLength,"%ld",strlen(responseBody));

        //allocate memory for response and write message to it
        response = malloc(strlen("HTTP/1.1 200 OK\r\nContent-Length: %s\r\n\r\n%s") + strlen(responseBody) + strlen(responseBodyLength));
        if (response == NULL) 
        { 
            perror("Memory not allocated"); 
            exit(EXIT_FAILURE);
        }
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
        if (statusCodeStr == NULL) 
        { 
            perror("Memory not allocated"); 
            exit(EXIT_FAILURE);
        }
        sprintf(statusCodeStr,"%d",statusCode);

        //allocate memory for response and write message to it
        response = malloc(strlen("HTTP/1.1 %d\r\nContent-Length: 0\r\n\r\n") + strlen(statusCodeStr));
        if (response == NULL) 
        { 
            perror("Memory not allocated"); 
            exit(EXIT_FAILURE);
        }
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
    if (allDestinationTimetable.timetableEntry == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }

    //initialise station components
    allDestinationTimetable.count = 0;

    allDestinationTimetable.longitude = malloc(strlen(timetable.longitude) + 1);
    if (allDestinationTimetable.longitude == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
    strcpy(allDestinationTimetable.longitude,timetable.longitude);

    allDestinationTimetable.latitude = malloc(strlen(timetable.latitude) + 1);
    if (allDestinationTimetable.latitude == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
    strcpy(allDestinationTimetable.latitude,timetable.latitude);

    //add all entries which lead to the required destination to newly created timetable
    for(int i = 0; i < timetable.count; i++)
    {
        timetableEntry entry = timetable.timetableEntry[i];

        if(strcmp(destination,entry.destination) == 0)
        {
            //add entry to timetable and resize the array
            allDestinationTimetable.timetableEntry = realloc(allDestinationTimetable.timetableEntry, (allDestinationTimetable.count + 1) * sizeof(timetableEntry));
            if (allDestinationTimetable.timetableEntry == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }
            allDestinationTimetable.timetableEntry[allDestinationTimetable.count] = entry;
            allDestinationTimetable.count++;
        }
    }

    return allDestinationTimetable;
}

//find the fastest route from a timetable of routes (provided it is after the aftertime)
char *find_fastest_route(Timetable allDestinationTimetable,char *after_time_str) 
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

#define MAX_BUFFER_SIZE 1024

void start_server(char* stationName, int browser_port, int query_port, char** neighbors, int num_neighbors) 
{
    //read file and get timetable
    char *filename = malloc(strlen("tt-%s") + strlen(stationName));
    sprintf(filename,"tt-%s",stationName);
    Timetable stationTimetable = read_timetable(filename);

    //store initial mtime of file
    struct stat filestat;
    stat(filename, &filestat);
    __time_t last_mtime = filestat.st_mtime;

    //get the current time (time the webpage was created) for use in calculations
    time_t rawtime;
    struct tm * currentTime;
    time(&rawtime);
    currentTime = localtime(&rawtime);

    //turn time into string
    char *afterTime = malloc(strlen(":") + 4);
    char *hour = malloc(3);
    char *minute = malloc(3);
    //add padding 0 if hour or minute < 10
    if (currentTime->tm_hour < 10) {
        sprintf (hour, "%02d", currentTime->tm_hour);
    } else {
        sprintf (hour, "%d", currentTime->tm_hour);
    } if (currentTime->tm_min < 10) {
        sprintf (minute, "%02d", currentTime->tm_min);
    } else {
        sprintf (minute, "%d", currentTime->tm_min);
    }
    sprintf(afterTime,"%s:%s",hour, minute);

    //get filtered timetable
    Timetable filteredTimetable = filter_timetable(stationTimetable,afterTime);

    //declare variables required for tcp and udp
    int tcpServerSocket, newSocket, udpServerSocket;
    struct sockaddr_in tcp_server_addr, udp_server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int numbytes;
    char udpDatagram[MAX_BUFFER_SIZE];

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
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(tcpServerSocket, 5) < 0) 
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started for %s on browser port %d and query port %d\n", stationName, browser_port, query_port);

    //used for select
    fd_set readset;
    int maxFileDescriptor;

    while(1) 
    {
        //restat the timetable file
        stat(filename, &filestat);
        //if its been modified, reread the timetable, filter it, and update last_mtime
        if(filestat.st_mtime != last_mtime) 
        {
            stationTimetable = read_timetable(filename);
            filteredTimetable = filter_timetable(stationTimetable, afterTime);
            last_mtime = filestat.st_mtime;
            printf("Updated timetable info for %s\n", stationName);
        }

        //add to readset
        FD_CLR(tcpServerSocket,&readset);
        FD_CLR(udpServerSocket,&readset);
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

        //declare and initialise the timeout value (currently 10 seconds)
        struct timeval timeout;
        timeout.tv_sec  = 10;
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
                printf("Connection from %s to %s\n", inet_ntoa(client_addr.sin_addr), stationName);
                
                // Receive the query from the web interface
                char query[MAX_BUFFER_SIZE];
                memset(query, 0, sizeof(query));
                read(newSocket, query, sizeof(query));
                char* cut_query = strtok(query, "\n");
                printf("%s received query: %s\n", stationName, cut_query);

                //parse the destination
                char *destination = parse_destination(query);
                if(destination == NULL)
                {
                    // Clean up the connection
                    printf("Closed due to null destination\n");
                    close(newSocket);
                    continue;
                }

                //get the fastest route
                Timetable destinationTimetable = destination_timetable(filteredTimetable,destination);
                char* route = find_fastest_route(destinationTimetable, afterTime);

                bool neighbours_destination = false;
                //iterate through the unfiltered timetable
                for (int i = 0; i < stationTimetable.count; i++) {
                    //if queried destination is in the timetable
                    if (strcmp(destination, stationTimetable.timetableEntry[i].destination) == 0) {
                        neighbours_destination = true;
                    }
                }

                //if station does not neighbour the destination, send out UDP request
                if (!neighbours_destination) {
                    //TODO send UDP request to other stations here

                    //temp, will just report it
                    route = malloc(strlen("%s does not neighbour %s") + strlen(stationName) + strlen(destination));
                    sprintf(route,"%s does not neighbour %s", stationName, destination);
                }

                if(route == NULL)
                {
                    route = malloc(strlen("There is no journey from %s to %s leaving after %s today") + strlen(stationName) + strlen(destination) + strlen(afterTime));
                    sprintf(route,"There is no journey from %s to %s leaving after %s today",stationName,destination,afterTime);
                }
                
                // Format the response message with the timetable information
                char *responseBody = malloc(strlen("Fastest route to :%s\n%s") + strlen(route) +strlen(destination));
                if(responseBody == NULL)
                {
                    perror("Memory allocation failed");
                    exit(EXIT_FAILURE);
                }
                sprintf(responseBody,"Fastest route to %s:\n%s",destination,route);

                // Format the HTTP response
                char* response = generate_http_response(200, responseBody);

                // Send the response back to the web interface
                write(newSocket, response, strlen(response));

                // Clean up the connection
                printf("Closed after finding route\n");
                close(newSocket);
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
                printf("Received UDP data: %s\n", udpDatagram);

                //get components from datagram
                char *datagramParts = strtok(udpDatagram,"~");
                char *messageType = malloc(strlen(datagramParts) + 1);
                if (messageType == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(messageType,datagramParts);

                if(strcmp(messageType,"M"))
                {
                    //TODO
                }

                if(strcmp(messageType,"A"))
                {
                    //TODO
                }

                if(strcmp(messageType,"R"))
                {
                    //TODO
                }

                datagramParts = strtok(NULL,"~");
                char *sourceStation = malloc(strlen(datagramParts) + 1);
                if (sourceStation == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(sourceStation,datagramParts);

                datagramParts = strtok(NULL,"~");
                char *destStation = malloc(strlen(datagramParts) + 1);
                if (destStation == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(destStation,datagramParts);

                //if the destination is reached
                if(strcmp(destStation,stationName))
                {
                    //TODO
                }

                datagramParts = strtok(NULL,"~");
                char *journeyPorts = malloc(strlen(datagramParts) + 1);
                if (journeyPorts == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(journeyPorts,datagramParts);
                //add current port to journey ports -- TODO

                datagramParts = strtok(NULL,"~");
                char *currentTime = malloc(strlen(datagramParts) + 1);
                if (currentTime == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(currentTime,datagramParts);
                //send datagram out to all neighbours after this time --TODO

                datagramParts = strtok(NULL,"~");
                char *timeToLiveStr = malloc(strlen(datagramParts) + 1);
                if (timeToLiveStr == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(timeToLiveStr,datagramParts);
                int timeToLive = atoi(timeToLiveStr);
                if(timeToLive == 0)
                {
                    //DROP PACKET
                }
                timeToLive--;

                datagramParts = strtok(NULL,"~");
                char *routeSoFar = malloc(strlen(datagramParts) + 1);
                if (routeSoFar == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(routeSoFar,datagramParts);

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
    char** neighbors = calloc(1,sizeof(char *));

    //can have many neighbors so add all command line arguments after 3 into neighbors
    if(neighbors == NULL) 
    {
        perror("allocation failed");
        exit(EXIT_FAILURE);
    }
    int count = 0;
    for(int i = 4; i < argc; i++)
    {
        neighbors[count] = argv[i];
        neighbors = (char **)realloc(neighbors, (count + 1)*sizeof(char *));
        if(neighbors == NULL) 
        {
            perror("allocation failed");
            exit(EXIT_FAILURE);
        }
        count++;
    }

    // Start the server with the provided configuration
    start_server(station_name, browser_port, query_port, neighbors, count);

    return 0;
}
