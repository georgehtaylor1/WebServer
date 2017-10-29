#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include "helpers.h"
#include "server.h"

#define RECVBUFFSIZE 1024
#define CONNECTION_TIMEOUT 30000
#define CONNECTION_REQUEST_LIMIT 10

struct Client *new_client = NULL;
int sock_desc;

int connectionAlive, serverAlive;

void sig_handler(int signo) {
    if (signo == SIGINT)
        printf("Killing server\n");

    //closeClient(new_client);
    //close(sock_desc);

    connectionAlive = 0;
    serverAlive = 0;

    printf("Process terminated\n");

}

int error(char *msg) {
    perror(msg);
    return 1;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        error("Insufficient paramters supplied, usage: ./server <-d?> <port> <web directory>");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        error("Can't catch SIGINT");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "-d") == 0 && argc == 4) {

        // Set the process up to run as a daemon
        pid_t daemonid, sid;
        daemonid = fork();
        if (daemonid < 0)
            exit(EXIT_FAILURE);

        if (daemonid > 0)
            exit(EXIT_SUCCESS);

        // Create a new SID for the child process
        sid = setsid();
        if (sid < 0)
            exit(EXIT_FAILURE);

        // Change the current working directory
        if ((chdir("/")) < 0)
            exit(EXIT_FAILURE);

        // Detatch from the standard outputs
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

    } else if (argc != 3) {
        error("Insufficient parameters supplied, usage: ./server <-d?> <port> <web directory>");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    server_root_dir = argv[2];

    int counter = 0;
    sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_desc == -1) {
        error("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Setup the host listener
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sock_desc, (struct sockaddr *) &server, sizeof(server)) == -1) {
        error("Failed to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sock_desc, 2) == -1) {
        error("Failed to listen on socket");
        exit(EXIT_FAILURE);
    }

    serverAlive = 1;

    struct pollfd ufds[1];
    ufds[0].fd = sock_desc;
    ufds[0].events = POLLIN;

    while (serverAlive == 1) {
        // Connect a new client
        new_client = malloc(sizeof(struct Client));
        if (new_client == NULL) {
            perror("Failed to allocate memory for new client");
        }

        int pollVal = poll(ufds, 1, -1);
        if (pollVal > 0) {
            new_client->socket = accept(sock_desc, &new_client->sock_addr, &new_client->client_length);
            if (new_client->socket != -1) {
                new_client->id = counter++;
                int pid = fork();

                if (pid == -1) {
                    perror("Failed to fork new process");
                    closeClient(new_client);
                } else if (pid == 0) {
                    printf("Forked new client with id %d\n", new_client->id);
                    return serveClient(new_client);
                } else {
                    closeClient(new_client);
                }
            }
        }
    }
    printf("%d - Escaped!\n", getpid());
    close(sock_desc);
    if (new_client != NULL) free(new_client);
    return 0;
}


/**
 *
 * Serve a particular client while ever it's connection is alive
 * @param client The client to be served
 * @return Did the client serving complete correctly
 */
int serveClient(struct Client *client) {
    printf("%d: Serving client %d\n", getpid(), client->id);

    if (client->socket < 0) return error("Connection failed");

    printf("Connection success\n");

    connectionAlive = CONNECTION_REQUEST_LIMIT;

    while (connectionAlive > 0) {
        char *recBuff = malloc(sizeof(char) * RECVBUFFSIZE);
        if (recBuff == NULL) {
            return error("Failed to allocate memory for buffer");
        }

        char *headers = (connectionAlive > 1) ? "Connection: keep-alive\n" : "";

        printf("Waiting to receive\n");

        struct pollfd ufds[1];
        ufds[0].fd = client->socket;
        ufds[0].events = POLLIN;
            int pollVal = poll(ufds, 1, CONNECTION_TIMEOUT);

        if (pollVal > 0) {
            int buffLen = receive(client, &recBuff);
            if (buffLen != 0) {
                printf("\nReceived\n%s\nparsing request... \n", recBuff);
                struct HTTP_request *request = parse_request(client, recBuff, buffLen, headers);
                if (request != NULL) {

                    if (request->keep_alive != 1) {
                        connectionAlive = 0;
                    }

                    serve(client, request, headers);

                    free_request(request);
                    connectionAlive--;
                }
            } else {
                closeClient(client);
                free(recBuff);
                return error("An error occurred attempting to read from the socket");
            }
        } else if (pollVal == 0) {
            connectionAlive = 0;
        } else {
            perror("Failed to poll socket");
            connectionAlive = 0;
        }
        free(recBuff);

    }
    closeClient(client);
    return 0;
}

/**
 * Pull a single request off the socket
 * @param client
 * @param buff
 * @return The length of the buffer
 */
int receive(struct Client *client, char **buff) {

    struct pollfd ufds[1];
    ufds[0].fd = client->socket;
    ufds[0].events = POLLIN;

    int bufferSize;
    int buffLen = 0;
    int numBytes = RECVBUFFSIZE;
    int pollVal = poll(ufds, 1, -1);
    while (numBytes == RECVBUFFSIZE) {
        numBytes = recv(client->socket, *buff + buffLen, RECVBUFFSIZE, 0);
        if (numBytes == -1) {
            perror("Error recv()ing bytes");
            return 0;
        }
        buffLen += numBytes;
        bufferSize = buffLen + RECVBUFFSIZE;
        if ((*buff = realloc(*buff, bufferSize)) == NULL) {
            perror("Failed to realloc for receiving request");
            return 0;
        }
    }
    if (pollVal == -1 || (pollVal == 0 && buffLen == 0))
        return 0;

    return buffLen;

}

int closeClient(struct Client *client) {

    if (client == NULL) return error("Failed to close uninitialised client");

    printf("%d - Closing client %d...\n", getpid(), client->id);
    close(client->socket);
    free(client);
    return 0;
}