#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include "commonfunction.h"

/* Number of possible client and server arguments */
#define MAX_CLIENT_ARG 4
#define SERVER_ARG_1 2
#define SERVER_ARG_2 3

/**
 * Check usage errors for both client and server. Can either be called from
 * server or client side. However, both same error behaviours.
 * Usage error -> Incorrect number of args or unable to open authfile. 
 * argc is the number of command arguments.
 * authfile is the authentication file to be opened.
 * type signifies whether client or server is calling function
 *     1 -> client calling
 *     2 -> server calling
 * Exit with 1 if usage error
 */
void usage_error(int argc, char* authfile, int type) {
    FILE* auth = fopen(authfile, "r");
    if (type == CLIENT_CALL) { // client side calling function
        if (argc != MAX_CLIENT_ARG || auth == NULL) {
            fprintf(stderr, "Usage: client name authfile port\n");
            exit(ARG_ERROR);
        }
    } else if (type == SERVER_CALL) { // server side calling
        if ((argc != SERVER_ARG_1 && argc != SERVER_ARG_2) || auth == NULL) {
            fprintf(stderr, "Usage: server authfile [port]\n");
            exit(ARG_ERROR);
        }
    }
}

/**
 * Dynamically reads a stream per line and store it with enough memory. 
 * stream is the stream to read from i.e stdin.
 * Returns the whole line gotten from stream.
 */
char* read_line(FILE* stream) {
    // actualSize->physical size, fullSize->what to reallocate to
    int actualSize = 0, fullSize = 1;
    char* line, c;
    line = malloc(sizeof(char));

    while (1) {
        c = fgetc(stream);
        if (c == EOF && actualSize == 0) {
            free(line);
            return NULL;
        }

        if (actualSize == fullSize - 1) { // Note: -1 -> safe memory management
            fullSize *= 2; // If caught up, double memory
            line = realloc(line, fullSize * sizeof(char));
        }

        if (c == EOF || c == '\n') {
            // Reallocate to actualSize -> remove memory waste
            line = realloc(line, (actualSize + 1) * sizeof(char));
            line[actualSize] = '\0';
            break;
        }
        line[actualSize++] = c; // Storing c into a string
    }
    return line;
}

/**
 * Determines the authentication code which is on a single line. 
 * auth is a text file containing the authentication code.
 * Returns this authentication code, or "noauth" if the file is empty.
 * (Note: "noauth" -> no authentication(any clients can join)).
 */
char* get_auth_line(FILE* auth) {
    char* authLine = "noauth";// Empty file implies no authentication
    char* line;
    while ((line = read_line(auth)) != NULL) {
        if (line[0] != '\0') { // Ignores empty line
            authLine = line;
        }
    }
    return authLine;
}

/**
 * Sets up the address and socket prerequisites and establish connection on
 * IPv4 and check connections with a port.
 * Can either be called from client or server, which leads to different 
 * behaviours.
 * Server -> puts server  in passive mode
 * Client -> Same as server but not in passive mode
 * port is the port number of check.
 * type signifies whether server or client side is calling function.
 *     1 -> client calling
 *     2 -> server calling
 * Exit with 2 if addrinfo does not exist.
 */
struct addrinfo* addr_set_up(char* port, int type) {
    struct addrinfo* addressInfo = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    
    if (type == SERVER_CALL) { // -> Server calling function
        hints.ai_flags = AI_PASSIVE; // Listen on all IP addresses
    }

    int err;
    if ((err = getaddrinfo("localhost", port, &hints, &addressInfo))) {
        /* Checking if port is usable */
        freeaddrinfo(addressInfo);
        fprintf(stderr, "Communications error\n");
        exit(COM_ERROR);
    }
    return addressInfo;
}

/**
 * Creates a fake client structure with its relevant detail. Can either be 
 * called from client or server side leading to different behaviours.
 * Server -> adds everything else + firstClient
 * Client -> adds everything else + name
 * name is the name of client, auth is the authentication code, contact is the
 * socket connection, firstClient is the root client.
 * type signifies whether server is calling or client.
 *     1 -> client calling
 *     else -> server calling
 * returns the newly created client
 */
Client* client_create(char* name, char* auth, int contact,
        ClientInfo** firstClient, int type) {
    Client* details = malloc(sizeof(Client)); // Allocating space before add
    
    if (type == CLIENT_CALL) { // -> Client calling function
        details->name = name;
        details->nameFlag = 0; // Track if name negotiation is finished
    } else { // -> Server calling function
        details->firstClient = malloc(sizeof(ClientInfo*));
        details->firstClient = firstClient;
    }

    details->auth = auth;
    details->contact = contact;
    return details;
}
