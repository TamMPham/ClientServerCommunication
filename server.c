#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "commonfunction.h"

/* Non printables i.e < 32*/
#define NON_PRINTABLE 32

/* Type of message to be broadcasted to other clients */
#define MSG_TYPE 1
#define LEAVE_TYPE 2
#define ENTER_TYPE 3

/* Delay time after client sends SAY: command(microsecond) */
#define SAY_DELAY 100000

/**
 * Determines all clients in the chat and send them over to the client who
 * called the LIST: command. 
 * firstClient is the root client
 * write is to write to the client who called the *LIST: command
 */
void list_name(ClientInfo* firstClient, FILE* write) {
    size_t currentLength, nameLength;
    char* allNames = malloc(sizeof(char));
    allNames[0] = '\0'; // Removes garbage value

    for (ClientInfo* curr = firstClient; curr != NULL; curr = curr->next) {
        // Storing old length for reallocation
        currentLength = strlen(allNames); 
        nameLength = strlen(curr->name);
        // Reallocation -> old size + new elemnt size
        allNames = realloc(allNames, 
                (currentLength + nameLength + 1) * sizeof(char));
        strcat(allNames, curr->name);
        if (curr->next != NULL) { // -> No comma after last name
            strcat(allNames, ",");
        }
    }
    fprintf(write, "LIST:%s\n", allNames);
    fflush(write);
    free(allNames);
}

/**
 * Replaces the characters in a word that are non-printable with '?'.
 * word is the word to checked and replaced.
 * Returns the newly word with replaced '?'
 * Note(non-printable means < 32 Ascii value)
 */
char* convert_non_printables(char* word) {
    char* converted = malloc(strlen(word) * sizeof(char));;

    for (int i = 0; i < strlen(word); i++) {
        if (word[i] < NON_PRINTABLE) { // < 32 ascii
            converted[i] = '?';    
        } else {
            converted[i] = word[i];
        }
    }
    return converted;
}

/**
 * Finds the client with a given name.
 * firstClient is the root client
 * name is the name to search for
 * returns the found client.
 */
ClientInfo* find_client_info(ClientInfo* firstClient, char* name) {
    ClientInfo* match;
    for (ClientInfo* curr = firstClient; curr != NULL; curr = curr->next) {
        if (!strcmp(curr->name, name)) {
            match = curr;
        }
    }
    return match;
}

/**
 * Broadcasts a message, leave, enter commands to all the clients in the chat.
 * firstClient is the root client
 * name is the name of the broadcasting client
 * message is the message to be broadcasted(Note: only if command is MSG:)
 * type is the type of broadcasting command
 *     - 1 -> MSG:name:text
 *     - 2 -> LEAVE:name 
 *     - else -> ENTER:name
 */
void broadcast(ClientInfo* firstClient, char* name, char* message, int type) {
    for (ClientInfo* curr = firstClient; curr != NULL; curr = curr->next) {
        if (type == MSG_TYPE) { // -> MSG:name:text broadcast
            fprintf(curr->write, "MSG:%s:%s\n", name, message);
        } else if (type == LEAVE_TYPE) { // -> Leave:name broadcast
            fprintf(curr->write, "LEAVE:%s\n", name);
        } else { // -> ENTER:name broadcoast
            fprintf(curr->write, "ENTER:%s\n", name);
        }
        fflush(curr->write);
    }
}

/**
 * Handles the procedure when SAY: is sent from client.
 * Procedure:
 *     -Increment SAY: counters for both client and server
 *     -broadcast message to all clients
 *     -Sleep for 100ms
 * statNeeds is to keep track of server's statistics(i.e SAY: count)
 * id is to keep track of client's statistics(i.e SAY: count)
 * firstClient is the root client
 * convertName is the name of client after non-printables are converted
 * saveAction is the message after SAY: command
 */
void say_handler(Stat** statNeeds, ClientInfo* id, ClientInfo** firstClient,
        char* convertName, char* saveAction) {
    ((*statNeeds)->sayC)++; // Server's stat SAY: counter
    (id->say)++; // Client's stat SAY: counter
    char* message = convert_non_printables(saveAction);
    printf("%s: %s\n", convertName, message);
    fflush(stdout);
    broadcast(*firstClient, convertName, message, MSG_TYPE);
    free(message); 
    usleep(SAY_DELAY); // Sleep for 100ms
}

/**
 * Adds a client to a list in lexographical order of their name. Also will 
 * make the first one of the list as the root if there isn't one.
 * firstClient is the root client
 * name is the client's name to be added
 * contact is the socket connection to client
 * write is to write to client
 */
void add_client_info(ClientInfo** firstClient, char* name, int contact,
        FILE* write) {
    // Allocating before adding
    ClientInfo* newClient = malloc(sizeof(ClientInfo)); 

    newClient->name = name;
    newClient->contact = contact;
    newClient->write = write;
    // New client means they haven't said anything yet, hence 0.
    newClient->say = 0;
    newClient->kick = 0;
    newClient->list = 0;
    
    // No clients exists yet
    if (*firstClient == NULL) {
        *firstClient = newClient;
        return;
    } 

    // List insertion in lexographical order
    ClientInfo* prev;
    ClientInfo* curr = *firstClient; 

    if (strcmp(name, (*firstClient)->name) < 0) {
        // Name insertion comes before root client
        newClient->next = *firstClient;
        *firstClient = newClient;
        return;
    } 
   
    // Searching for lexographical order
    prev = curr;
    while (curr && strcmp(curr->name, name) < 0) {
        prev = curr;
        curr = curr->next;
    }
    
    // Check to see if at the end of list
    if (!curr) {
        prev->next = newClient;
    } else { // Normal insertion in between 2 elements
        newClient->next = curr;
        prev->next = newClient;
    }
}

/**
 * Removes the first occurence of a client given a specified name.
 * firstClient is the root client
 * name is the client's name to be removed
 * (Note: if no client exists -> do nothing)
 */
void remove_client_info(ClientInfo** firstClient, char* name) {
    ClientInfo* toRemove;
    if (*firstClient == NULL) {
        return;
    }
    // If specified client is the root
    if (strcmp((*firstClient)->name, name) == 0) {
        toRemove = *firstClient; // searches for specified client
        *firstClient = (*firstClient)->next;
        // Handles deallocation
        close(toRemove->contact);
        fclose(toRemove->write);
        free(toRemove);
        return;
    }
    // Normal prodcedure if name is not root
    for (ClientInfo* curr = *firstClient; curr->next != NULL;
            curr = curr->next) {
        if (strcmp(curr->next->name, name) == 0) {
            toRemove = curr->next; // Searches for client to be removed
            curr->next = curr->next->next;
            // Handles deallocation 
            close(toRemove->contact);
            fclose(toRemove->write);
            free(toRemove);
            return;
        }
    }
}

/**
 * Handle clients who failed authentication or name negotiation, cleaning 
 * up resources/memories.
 * contact is socket connection to client, linked to read
 * contact2 is a duplicate of contact, linked to write
 * write is to write to client
 * read is to read response back from client
 * Exits failed client thread with error code of 2
 */
void client_cleanup(int contact, int contact2, FILE* write, FILE* read) {
    close(contact);
    close(contact2);
    fclose(read);
    fclose(write);
    pthread_exit((void*)COM_ERROR);
}

/**
 * Handles the LEAVE: command sent by client to the server, which removes 
 * client from the chat with appropriate protocol.
 * firstClient is the root client
 * name is client's name that is leaving
 * contact is socket connection to client, allowing reading to client
 * contact2 is a duplicate of contact, allowing writing to client
 */
void leave_procedure(ClientInfo** firstClient, char* name, int contact,
        int contact2) {
    printf("(%s has left the chat)\n", name);
    fflush(stdout);
    remove_client_info(firstClient, name);
    broadcast(*firstClient, name, NULL, LEAVE_TYPE);
    close(contact);
    close(contact2);
}

/**
 * Kick a client from the chat with a specified name.
 * firstClient is the root client
 * name is the name of client to be kicked
 * (Note: if name doesn't exist -> do nothing)
 */
void kick_named_client(ClientInfo** firstClient, char* name) {
    for (ClientInfo* curr = *firstClient; curr != NULL; curr = curr->next) {
        if (!strcmp(curr->name, name)) { // Searching for the right name
            fprintf(curr->write, "KICK:\n");
            fflush(curr->write);
            remove_client_info(firstClient, name);
            printf("(%s has left the chat)\n", name);
            fflush(stdout);
            broadcast(*firstClient, name, NULL, LEAVE_TYPE);
            return;
        }
    }
}

/**
 * Checks if a specified client's name exists in thbe system.
 * firstClient is the root client
 * name is the client's name to check for
 * Returns 1 if client exists, 0 if doesn't
 */
int name_exist(ClientInfo* firstClient, char* name) {
    ClientInfo* tempFirst = firstClient;
    while (tempFirst != NULL) {
        if (!strcmp(tempFirst->name, name)) { // Name matching
            return 1; 
        }
        tempFirst = tempFirst->next;
    }
    return 0;
}

/**
 * Gets a client's name. if their returned name is empty, sends NAME_TAKEN: 
 * command and get their name again.
 * statNeeds is to keep track of server's total statistics(i.e say counter...)
 * contact is socket connection to client, linked to read
 * contact2 is a duplicate of contact, linked to write
 * write is to write to client
 * read is to read response back from client
 * Returns the client's name returned back
 * (Note: exit with error code 2 if client fails name negotiation)
 */
char* extract_name(Stat** statNeeds, int contact, int contact2, FILE* write,
        FILE* read) {
    fflush(write); // for NAME_TAKEN: to go through 
    fprintf(write, "WHO:\n");
    fflush(write);
    char* response = read_line(read);
    char* clientName;
    
    if (response == NULL) {
        client_cleanup(contact, contact2, write, read);
    }

    response = strtok_r(response, ":", &clientName);
    /* response in form NAME:name*/
    if (response != NULL) {
        if (!strcmp(response, "NAME")) { // Total server counter for SIGHUP
            ((*statNeeds)->nameC)++;
        }
    } 

    if (clientName[0] == '\0') { // If name is empty -> NAME_TAKEN: -> new name
        fprintf(write, "NAME_TAKEN:\n");
        fflush(write);
        fprintf(write, "WHO:\n");
        fflush(write);
        
        response = read_line(read); 
        response = strtok_r(response, ":", &clientName); // Extract new name
        if (response != NULL) {
            if (!strcmp(response, "NAME")) { // Server counter for SIGHUP stat
                ((*statNeeds)->nameC)++;
            }
        }
    }
    return clientName;
}

/**
 * Handles the name negotiation with a client which is as followed:
 *     -Get client's name
 *     -Sends NAME_TAKEN: and get their name back until they return a unique
 *     name 
 *     -sends OK: if the process is done
 *     -sends ENTER:name to all clients
 * firstClient is the root client
 * statNeeds is to keep track of server's total statistics(i.e say count)
 * contact is the socket connection to server, linked to read
 * contact2 is a duplicate of contact, linked to write
 * write is to write to client
 * read is to read response from client
 * Returns the unique client name. 
 */
char* name_handler(ClientInfo** firstClient, Stat** statNeeds, int contact,
        int contact2, FILE* write, FILE* read) {
    char* clientName;
    clientName = extract_name(statNeeds, contact, contact2, write, read);
    
    while (name_exist(*firstClient, clientName)) { // Update name if duplicated
        fprintf(write, "NAME_TAKEN:\n");
        // flushing inside extract_name
        clientName = extract_name(statNeeds, contact, contact2, write, read);
    }

    // After finding a unique name
    add_client_info(firstClient, clientName, contact, write); // Adds client
    fprintf(write, "OK:\n");
    fflush(write);
    printf("(%s has entered the chat)\n", clientName);
    fflush(stdout);
    // Broadcasts ENTER:name to all other clients
    broadcast(*firstClient, clientName, NULL, ENTER_TYPE); 
    return clientName;
}

/**
 * Authentication check with the client. If matches, then client gets added
 * to the client. Else, client's info gets cleaned up. 
 * statNeeds is to keep track of server's total statistics(i.e say count)
 * contact is the socket connection to client
 * contact2 is a duplicated from contact
 * serverAuth is the server's authentication code
 * toClient is to read response from client
 * fromClient is to write to client
 * Exit with error code of 2 if client fails authentication
 * (Note: if server is in no authentication mode, any client can join with any 
 * authentication code)
 */
void auth_check(Stat** statNeeds, int contact, int contact2, char* serverAuth, 
        FILE* toClient, FILE* fromClient) {
    char* line;
    fprintf(toClient, "AUTH:\n");
    fflush(toClient);
    line = read_line(fromClient);
    char* clientAuth;
    char* saveClientAuth;
    clientAuth = strtok_r(line, ":", &saveClientAuth);
    
    if (clientAuth == NULL) {
        pthread_exit((void*)COM_ERROR);
    }

    if (!strcmp(clientAuth, "AUTH")) { // Track server total stat for SIGHUP
        ((*statNeeds)->authC)++;
    }
    
    // If auth code matches or no server auth needed
    if (!strcmp(serverAuth, saveClientAuth) || !strcmp(serverAuth, "noauth")) {
        fprintf(toClient, "OK:\n");
        fflush(toClient);
        return;
    }
    // If auth code doesn't match, clean up client
    client_cleanup(contact, contact2, toClient, fromClient);
}

/**
 * Handles all the protocol of each client entering the chat. Client will
 * need to go through these processes:
 *     -Authentication check
 *     -Name negotiation
 * If client passes, then they can freely chat in the server and call 
 * appropriate commands. Commands client's can use:
 *     -SAY:
 *     -LIST:
 *     -KICK:
 *     -LEAVE:
 * (Note: any invalid commands are silently ignored by the server)
 * If a client disconnects from the server, all their info gets erased.
 * 
 * details contains client's information which consists of:
 *     -authentication code
 *     -socket connection
 *     -total server stat's tracker(i.e say:)
 * Exit with 2 if kicked otherwise 0
 */
void* client_handler(void* details) {
    Client* detail = (Client*)details; 
    char* authLine = (*detail).auth;
    int contact = (*detail).contact, contact2 = dup(contact);
    FILE* read = fdopen(contact, "r"), *write = fdopen(contact2, "w");
    Stat** statNeeds = detail->statistics; // For total server statistics
    char* name, *convertName;
    // Authentication check and name negotiation
    auth_check(statNeeds, contact, contact2, authLine, write, read);
    pthread_mutex_lock(&(detail->lock));
    name = name_handler(detail->firstClient, statNeeds, contact, contact2,
            write, read);
    convertName = convert_non_printables(name); // < 32 Ascii
    ClientInfo* id = find_client_info(*(detail->firstClient), name); 
    pthread_mutex_unlock(&(detail->lock));
    
    char* response, *saveAction, *action; 
    while ((response = read_line(read)) != NULL) {
        pthread_mutex_lock(&(detail->lock));
        action = strtok_r(response, ":", &saveAction); // Extract responses
        if (!strcmp(action, "SAY")) {
            say_handler(statNeeds, id, detail->firstClient, convertName,
                    saveAction); // Handles SAY: command
        } else if (!strcmp(action, "LIST")) {
            ((*statNeeds)->listC)++; // For server stat
            (id->list)++; // For client stat
            list_name(*(detail->firstClient), write);
        } else if (!strcmp(action, "KICK")) {
            ((*statNeeds)->kickC)++; // For server stat
            (id->kick)++; // For client stat
            kick_named_client(detail->firstClient, saveAction);

            if (!strcmp(saveAction, name)) {
                pthread_exit((void*)COM_ERROR);
            }
        } else if (!strcmp(action, "LEAVE")) {
            ((*statNeeds)->leaveC)++; // For server stat
            leave_procedure(detail->firstClient, name, contact, contact2);
            pthread_exit((void*)NORM_EXIT);
        }
        pthread_mutex_unlock(&(detail->lock));
    }
    pthread_mutex_lock(&(detail->lock));
    if (name_exist(*(detail->firstClient), name)) {
        leave_procedure(detail->firstClient, name, contact, contact2);
    }
    pthread_mutex_unlock(&(detail->lock));
    pthread_exit((void*)NORM_EXIT);
}

/**
 * Actively listens for any client trying to connect to the server. Also prints
 * out the port number that is listening to.
 * port is the port to listen from
 * Returns the socket connection with the client trying to join 
 */
int client_listen(char* port) {
    struct addrinfo* addressInfo = addr_set_up(port, SERVER_CALL);
    int clientConnect = socket(AF_INET, SOCK_STREAM, 0);
    
    // Allow socket to be reused immediately
    int optVal = 1;
    setsockopt(clientConnect, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int));
    
    // Binding server to a specific port and listen
    if (bind(clientConnect, addressInfo->ai_addr, sizeof(struct sockaddr))
            < 0) {
        fprintf(stderr, "Communications error\n");
        exit(COM_ERROR);
    }

    // Determine ephemeral port chosen
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    getsockname(clientConnect, (struct sockaddr*)&addr, &len);
    fprintf(stderr, "%u\n", ntohs(addr.sin_port));
    
    listen(clientConnect, SOMAXCONN);// Listens for client connection
    return clientConnect;
}

/**
 * Process each client trying to connect and create a separate thread for
 * each one of them in the system which will follow a protocol at a later
 * stage.
 * connection is the socket connection of client
 * serverAuthLine is the authentication code of server
 * firstClient is the root client
 * statNeeds is to keep track of total server stats(i.e say count)
 */
void process_clients(int connection, char* serverAuthLine,
        ClientInfo** firstClient, Stat** statNeeds) {
    int clientComm;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    // Creating one lock for all clients
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    while (1) { // processing clients whenever they join
        fromAddrSize = sizeof(struct sockaddr_in);
        clientComm = accept(connection, (struct sockaddr*)&fromAddr,
                &fromAddrSize);
        /* Creating required data before passing into thread, function doesn't
        lock or server stat. (Note: will be add later below) 
        */
        Client* details = client_create(NULL, serverAuthLine, clientComm,
                firstClient, 2); 
        // Add to client's detail to keep track of server stats(SAY: count..)
        details->statistics = malloc(sizeof(Stat*));
        details->statistics = statNeeds;
        // Giving client the lock
        details->lock = lock;
        
        pthread_t clientId;
        pthread_create(&clientId, NULL, client_handler, details);
        pthread_detach(clientId);
    }
}

/**
 * Handles the SIGHUP signal sent to server with appropriate protocol
 *     -output all say, kick, list counts of all clients in the chat
 *     -output overall server stats of auth, name, say, kick, list, leave count
 * stats is all the servers stats above
 */
void* server_stats(void* stats) {
    Stat* statNeeds = (Stat*)stats; // For server's info
    sigset_t* signalSet = (*statNeeds).signalSet; // Contains only for SIGHUP
    ClientInfo** firstClient = (*statNeeds).firstClient; // For client's info

    int sighup;
    for (;;) { 
        sigwait(signalSet, &sighup); // Waiting for SIGHUP 

        ClientInfo* tempFirst = *firstClient;
        fprintf(stderr, "@CLIENTS@\n"); // Printing client's data
        while (tempFirst != NULL) { 
            fprintf(stderr, "%s:SAY:%d:KICK:%d:LIST:%d\n",tempFirst->name,
                    tempFirst->say, tempFirst->kick, tempFirst->list);
            tempFirst = tempFirst->next;
        }

        fprintf(stderr, "@SERVER@\n"); // Printing server's data
        fprintf(stderr, "server:AUTH:%d:NAME:%d:SAY:%d:KICK:%d:LIST:%d:"
                "LEAVE:%d\n", statNeeds->authC, statNeeds->nameC,
                statNeeds->sayC, statNeeds->kickC, statNeeds->listC,
                statNeeds->leaveC);
    }
}

int main(int argc, char* argv[]) {
    usage_error(argc, argv[1], SERVER_CALL);
    int connection;
    char* port;
    if (argc == 3) {
        port = argv[2];
    } else {
        port = "0";
    }
    ClientInfo* firstClient = NULL;
    
    struct sigaction ignore;
    ignore.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &ignore, NULL);
    
    pthread_t sighupCatch;
    sigset_t signalSet;
    sigemptyset(&signalSet);
    sigaddset(&signalSet, SIGHUP);

    Stat* statNeeds = malloc(sizeof(Stat));
    statNeeds->authC = 0;
    statNeeds->nameC = 0;
    statNeeds->sayC = 0;
    statNeeds->kickC = 0;
    statNeeds->listC = 0;
    statNeeds->leaveC = 0;

    statNeeds->signalSet = &signalSet;
    statNeeds->firstClient = &firstClient;
    pthread_sigmask(SIG_BLOCK, &signalSet, NULL);
    pthread_create(&sighupCatch, NULL, &server_stats, statNeeds);
     
    FILE* authentication = fopen(argv[1], "r");
    char* authLine = get_auth_line(authentication);
    connection = client_listen(port);
    process_clients(connection, authLine, &firstClient, &statNeeds);

    return NORM_EXIT;   
}
