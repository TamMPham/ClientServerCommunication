#ifndef _COMMONFUNCTION_H
#define _COMMONFUNCTION_H
#include <stdio.h>
#include <pthread.h>
#include <signal.h>

/* Whether common function is called from client or server side */
#define CLIENT_CALL 1
#define SERVER_CALL 2

/* Exit codes */
#define NORM_EXIT 0
#define ARG_ERROR 1
#define COM_ERROR 2
#define KICKED_EXIT 3
#define AUTH_ERROR 4

typedef struct Stat {
    int authC;
    int nameC;
    int sayC;
    int kickC;
    int listC;
    int leaveC;
    sigset_t* signalSet;
    struct ClientInfo** firstClient;
} Stat;

typedef struct Client {
    char* name;
    char* auth;
    int contact;
    struct ClientInfo** firstClient;
    struct Stat** statistics;
    int nameFlag;
    pthread_mutex_t lock;
} Client;

typedef struct ClientInfo {
    char* name;
    int contact;
    FILE* write;
    int say;
    int kick;
    int list;
    struct ClientInfo* next;
} ClientInfo;

void usage_error(int argc, char* authfile, int type);

char* read_line(FILE* stream);

char* get_auth_line(FILE* auth);

struct addrinfo* addr_set_up(char* port, int type);

Client* client_create(char* name, char* auth, int contact,
        ClientInfo** firstClient, int type);

#endif
