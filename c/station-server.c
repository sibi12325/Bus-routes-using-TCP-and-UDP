#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>

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

                perror(destination);

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
    timetableEntry **timetableEntry;
}Timetable;

//read the file and create the timetable
Timetable* read_timetable(const char* filename) 
{
    //open the file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("file couldnt be opened");
        exit(EXIT_FAILURE);
    }

    //allocate memory for timetable and timetable array
    Timetable* timetable = malloc(sizeof(Timetable));
    if (timetable == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
    timetable->timetableEntry = calloc(1,sizeof(timetableEntry));
    if (timetable->timetableEntry == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }

    //initialise count
    timetable->count = 0;

    //iterate through each line of the file
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) 
    {
        //skip lines starting with #
        if (line[0] != '#') 
        {
            //create new empty entry
            timetableEntry* entry = malloc(sizeof(timetableEntry));
            if (entry == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }


            char* departureInfoParts = strtok(line, ",");
            int count = 0;

            if(strlen(departureInfoParts) > 5)
            {
                continue;
            }

            //create array to store departure info
            char **departureInfo = malloc(4*sizeof(char *));
            departureInfo[0] = departureInfoParts;

            //keep splitting string until you cant
            while(departureInfoParts != NULL)
            {
                //move on to next split string
                departureInfoParts = strtok(NULL, ",");

                //dont include last departureInfoPart as it is the destination
                if(count < 4)
                {
                    departureInfo[count] = departureInfoParts;
                    char *errormsg2 = (char *)malloc(80);
                    sprintf(errormsg2,"current part %d: %s",count, departureInfoParts);
                    perror(errormsg2);
                }
                count += 1;
            }

            //if this line contains the departure info it would have 5 components
            if(count == 4)
            {
                //allocate memory and copy destination into entry
                entry->destination = malloc(sizeof(departureInfoParts));
                if (entry->destination == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                strcpy(entry->destination,departureInfoParts); //last part of split string is always destination

                //store departure info into appropriate array
                entry->departureInfo = departureInfo;

                //add entry to timetable and resize the array
                timetable->timetableEntry[timetable->count] = entry;
                timetable->timetableEntry = (timetableEntry **)realloc(timetable->timetableEntry, (timetable->count + 1) * sizeof(timetableEntry));
                if (timetable->timetableEntry == NULL) 
                { 
                    perror("Memory not allocated"); 
                    exit(EXIT_FAILURE);
                }
                timetable->count++;
            }
        }
    }

    //close file and return the timetable
    fclose(file);
    return timetable;
}

int strptime(const char* timeString, const char* format, struct tm* time) 
{
    int hour, minute;
    if (sscanf(timeString, format, &hour, &minute) != 2) 
    {
        char *errorMsg = (char *)malloc(strlen("hour : %d, minute %d") + 50);
        sprintf(errorMsg,"hour : %d, minute %d",hour,minute);
        perror(errorMsg);  // Error
        exit(EXIT_FAILURE);
    }

    time->tm_hour = hour;
    time->tm_min = minute;

    
    char *errorMsg = (char *)malloc(strlen("hour : %d, minute %d") + 50);
    sprintf(errorMsg,"hour : %d, minute %d",time->tm_hour,time->tm_min);
    perror(errorMsg);
    
    return 0;
}

Timetable *filter_timetable(Timetable* timetable, char* cuttOffTimeStr) 
{
    struct tm cuttOffTime;
    strptime(cuttOffTimeStr, "%d:%d", &cuttOffTime);

    //allocate memory for timetable and timetable array
    Timetable* filteredTimetable = malloc(sizeof(Timetable));
    if (filteredTimetable == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
    filteredTimetable->timetableEntry = calloc(1,sizeof(timetableEntry));
    if (filteredTimetable->timetableEntry == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }

    //initialise count
    filteredTimetable->count = 0;

    //iterate through each entry in timetable
    for(int i = 0; i < timetable->count; i++)
    {
        timetableEntry *entry = timetable->timetableEntry[i];

        //assume first element of departure info is departure time
        char *departureTimeStr = entry->departureInfo[0];
        struct tm departureTime;
        strptime(departureTimeStr, "%d:%d",&departureTime);
        
        //Compare the departure time with the cutoff time
        if (mktime(&departureTime) >= mktime(&cuttOffTime))
        {
            //create new empty entry
            timetableEntry* filteredEntry = malloc(sizeof(timetableEntry));
            if (filteredEntry == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }
            
            //copy filtered components into new empty entry
            filteredEntry->destination = malloc(sizeof(entry->destination));
            if (filteredEntry->destination == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }
            strcpy(filteredEntry->destination,entry->destination);
            filteredEntry->departureInfo = entry->departureInfo;

            //add entry to filtered timetable and resize
            filteredTimetable->timetableEntry[filteredTimetable->count] = filteredEntry;
            filteredTimetable->timetableEntry = (timetableEntry **)realloc(filteredTimetable->timetableEntry, (filteredTimetable->count + 1)*sizeof(timetableEntry));
            filteredTimetable->count++;
            
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
        response = malloc(strlen("HTTP/1.1 200 OK\r\nContent-Length: \r\n\r\n") + strlen(responseBody) + strlen(responseBodyLength));
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
        response = malloc(strlen("HTTP/1.1 \r\nContent-Length: 0\r\n\r\n") + strlen(statusCodeStr));
        if (response == NULL) 
        { 
            perror("Memory not allocated"); 
            exit(EXIT_FAILURE);
        }
        sprintf(response, "HTTP/1.1 %d\r\nContent-Length: 0\r\n\r\n", statusCode);
    }

    return response;
}

char *find_fastest_route(Timetable *timetable, char *destination, char *after_time_str) 
{
    //create and allocate memory for timetable containing entries leading only to destination
    Timetable *allDestinationTimetable = malloc(sizeof(Timetable));
    if (allDestinationTimetable == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
    allDestinationTimetable->timetableEntry = calloc(1, sizeof(timetableEntry));
    if (allDestinationTimetable->timetableEntry == NULL) 
    { 
        perror("Memory not allocated"); 
        exit(EXIT_FAILURE);
    }
    allDestinationTimetable->count = 0;


    //add all entries which lead to the required destination to newly created timetable
    for(int i = 0; i < timetable->count; i++)
    {
        timetableEntry *entry = timetable->timetableEntry[i];

        if(strcmp(destination,entry->destination) == 0)
        {
            allDestinationTimetable->timetableEntry[allDestinationTimetable->count] = entry;
            allDestinationTimetable->timetableEntry = (timetableEntry **)realloc(allDestinationTimetable->timetableEntry, (allDestinationTimetable->count + 1)*sizeof(timetableEntry));
            if (allDestinationTimetable->timetableEntry == NULL) 
            { 
                perror("Memory not allocated"); 
                exit(EXIT_FAILURE);
            }
            allDestinationTimetable->count++;
        }
    }


    //turn after time from string into struct tm object
    struct tm after_time;
    strptime(after_time_str, "%d:%d", &after_time);

    //initial conditions
    char* fastestRoute = NULL;
    double fastestDuration = 0.0;

    //iterate through every destination timetable route
    for(int i = 0; i < allDestinationTimetable->count; i++)
    {
        timetableEntry *entry = allDestinationTimetable->timetableEntry[i];

        //convert to struct tm objects from string
        struct tm departureTime, arrivalTime;
        strptime(entry->departureInfo[0], "%d:%d", &departureTime);
        strptime(entry->departureInfo[3], "%d:%d", &arrivalTime);

        //calculate duration
        double duration = difftime(mktime(&departureTime), mktime(&after_time)) + difftime(mktime(&arrivalTime), mktime(&departureTime));

        //find the fastest route
        if (fastestRoute == NULL || duration < fastestDuration) 
        {
            char *route = malloc(strlen("('', '', '', '')") + strlen(entry->departureInfo[0]) + strlen(entry->departureInfo[1])+ strlen(entry->departureInfo[2])+ strlen(entry->departureInfo[3]));
            sprintf(route,"('%s', '%s', '%s', '%s')",entry->departureInfo[0],entry->departureInfo[1],entry->departureInfo[2],entry->departureInfo[3]);
            fastestRoute = malloc(sizeof(route));
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
    int server_socket, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Create a TCP/IP socket for handling queries from the web interface
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the specified port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(browser_port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0) 
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    //read file and get timetable
    char *filename = malloc(strlen("tt-") + strlen(stationName));
    sprintf(filename,"tt-%s",stationName);
    Timetable *stationTimetable = read_timetable(filename);

    printf("Server started for %s on browser port %d and query port %d\n", stationName, browser_port, query_port);

    while(1) 
    {
        printf("Waiting for a connection from the web interface...\n");

        // Accept a new connection
        if ((new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len)) < 0) 
        {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        printf("Connection from %s\n", inet_ntoa(client_addr.sin_addr));

        // Receive the query from the web interface
        char query[MAX_BUFFER_SIZE];
        memset(query, 0, sizeof(query));
        read(new_socket, query, sizeof(query));
        printf("Received query: %s\n", query);

        //get the filtered timetable
        char *destination = parse_destination(query);
        char *afterTime = "10:00";
        Timetable *filteredTimetable = filter_timetable(stationTimetable, afterTime);

        //get the fastest route
        char *route = find_fastest_route(filteredTimetable, destination, afterTime);
        
        // Format the response message with the timetable information
        char *response_body = malloc(strlen("Fastest route to :\n" + strlen(route) +strlen(destination)));
        sprintf(response_body,"Fastest route to %s:\n%s",destination,route);

        // Format the HTTP response
        char* response = generate_http_response(200, response_body);

        // Send the response back to the web interface
        write(new_socket, response, strlen(response));

        // Clean up the connection
        printf("Closed\n");
        close(new_socket);
    }
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
