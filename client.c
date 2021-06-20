#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <pthread.h>
#include <math.h>
#include "commonfunction.h"

#define DELAY 100000 
#define NAME_DONE 1

/**
 * Returns the name of client with the case where duplicate exists
 * name is the name of the client i.e Fred
 * nameCounter is the counter for the client if NAME_TAKEN is sent
 */
char* get_name(char* name, int nameCounter) {
    // Length of nameCounter i.e 234 -> length = 3
    int nLength = (int)(abs(nameCounter) ? log10(abs(nameCounter)) + 1 : 1);
    char* actualName = malloc(sizeof(char));
    actualName = realloc(actualName,
            (strlen(name) + nLength + 1) * sizeof(char));
    actualName[0] = '\0'; // Note: null terminator to remove garbage value
    strcat(actualName, name);
    // Converting int -> string
    char* number = malloc(nLength * sizeof(char));
    sprintf(number, "%d", nameCounter);
    if (nameCounter > -1) { // Note: nameCounter > -1 if NAME_TAKEN is sent
        strcat(actualName, number);
    }
    free(number);
    return actualName;
}

/**
 * Handles client deallocation process after connection is terminated by 
 * server.
 * read is for reading from server
 * write is for writing to server
 * details contains socket connection with server and name of client i.e Fred
 *
 */
void free_client(FILE* read, FILE* write, Client* details) {
    fclose(read);
    fclose(write);
    free(details);
}

/**
 * Actively reads commands coming through from the server side and process
 * each command(AUTH:, NAME:, ENTER:, LEAVE:, MSG:, KICK:).If invaid command,
 * do nothing.
 * details contains socket connection with server and name of client i.e Fred
 * Exit with 3 if kicked by server.
 * Exit with 2 if loses connection with server.
 */
void* from_server(void* details) {
    Client* info = (Client*)details;
    int contact2 = dup(info->contact);
    FILE* read = fdopen(info->contact, "r"), *write = fdopen(contact2, "w");
    char* line, *sayName;
    char* baseName = info->name;
    int nameCounter = -1;// Starts at -1 -> 0 ->... when NAME_TAKEN:
    pthread_mutex_lock(&(info->lock));

    while ((line = read_line(read)) != NULL) {
        char* saveRequest; // To store everything after ':'
        char* request = strtok_r(line, ":", &saveRequest);
        if (!strcmp(request, "AUTH")) {  
            fprintf(write, "AUTH:%s\n",info->auth);
            fflush(write);
            line = read_line(read); // Handles authentication failure
            if (line == NULL) {
                fprintf(stderr, "Authentication error\n");
                free_client(read, write, details);
                exit(AUTH_ERROR);
            }
        } else if (!strcmp(request, "OK")) {
            (info->nameFlag)++; // 2 -> implies name negotiation is done
            pthread_mutex_unlock(&(info->lock));
        } else if (!strcmp(request, "WHO")) {
            fprintf(write, "NAME:%s\n", info->name);  
        } else if (!strcmp(request, "NAME_TAKEN")) {
            nameCounter++;
            char* clientName = get_name(baseName, nameCounter);
            strcpy(info->name, clientName);
            free(clientName);
        } else if (!strcmp(request, "ENTER")) {
            printf("(%s has entered the chat)\n", saveRequest);
        } else if (!strcmp(request, "LEAVE")) {
            printf("(%s has left the chat)\n", saveRequest);
        } else if (!strcmp(request, "MSG")) {
            sayName = strtok_r(NULL, ":", &saveRequest);
            printf("%s: %s\n", sayName, saveRequest);
        } else if (!strcmp(request, "KICK")) {
            fprintf(stderr, "Kicked\n");
            exit(KICKED_EXIT);
        } else if (!strcmp(request, "LIST")) {
            printf("(current chatters: %s)\n", saveRequest);
        }
        fflush(write);
    }
    usleep(DELAY); // When client loses connection with server
    fprintf(stderr, "Communications error\n");
    free_client(read, write, details);
    exit(COM_ERROR);
}

/**
 * Actively reads client commands and send them to server. If command 
 * begins with '*', send them whole(not including*) i.e *KICK: -> KICK:.Else,
 * send to server as a "SAY:" command. i.e Hello -> "SAY:Hello".
 * Exit with 0 if loses connection with server.
 */
void* to_server(void* details) {
    Client* info = (Client*)details;
    FILE* write = fdopen(info->contact, "w");
    char* commandLine;
    usleep(DELAY);

    while ((commandLine = read_line(stdin)) != NULL && (info->nameFlag)) {
        if (commandLine[0] == '*') {
            commandLine++; // Remove '*' before sending to server
            fprintf(write, "%s\n", commandLine);      
            fflush(write);
        } else {
            fprintf(write, "SAY:%s\n", commandLine);
            fflush(write);
        }
    }

    usleep(DELAY);
    fclose(write);
    free(details);
    exit(NORM_EXIT);
}

int main(int argc, char* argv[]) {
    usage_error(argc, argv[2], CLIENT_CALL); 
    char* port = argv[3];
    struct addrinfo* addressInfo = addr_set_up(port, 1);
    
    int connection = socket(AF_INET, SOCK_STREAM, 0);
    if ((connect(connection, addressInfo->ai_addr,
            sizeof(struct sockaddr)))) {
        fprintf(stderr, "Communications error\n");
        return COM_ERROR;
    }
    
    FILE* auth = fopen(argv[2], "r");
    char* authLine = get_auth_line(auth);
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    Client* details = client_create(argv[1], authLine, connection, NULL, 1);
    details->lock = lock;
    
    pthread_t toServerId;
    pthread_t fromServerId;
    pthread_create(&toServerId, 0, to_server, details);
    pthread_create(&fromServerId, 0, from_server, details);
    pthread_join(toServerId, NULL);
    pthread_join(fromServerId, NULL);

    pthread_mutex_destroy(&lock);
    return NORM_EXIT;
}
