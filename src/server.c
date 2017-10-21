#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "helpers.h"
#include "server.h"

const int recvBuffSize = 2048;

int main(int argc, char **argv) {

    if(argc < 2){
        perror("Insufficient paramters supplied, usage: ./server <-d?> <port> <web directory>");
        return 1;
    }

    if(strcmp(argv[1], "-d") == 0 && argc == 4) {

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

    } else if (argc != 3){
        perror("Insufficient paramters supplied, usage: ./server <-d?> <port> <web directory>");
        return 1;
    }

    int port = atoi(argv[1]);
    server_root_dir = argv[2];

    printf("Not even here\n");

    int counter = 0;
    int sock_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_desc == -1) {
        printf("Failed to create socket\n");
        return 1;
    }

    // Setup the host listener
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sock_desc, (struct sockaddr *) &server, sizeof(server)) == -1) {
        printf("Failed to bind\n");
        return 1;
    }

    printf("%d: Listening\n", getpid());
    listen(sock_desc, 2);

    int server_alive = 1;

    while (server_alive == 1) {
        // Connect a new client
        struct Client *new_client;
        new_client = malloc(sizeof(struct Client));
        new_client->socket = accept(sock_desc, &new_client->sock_addr, &new_client->client_length);
        new_client->id = counter++;
        int pid = fork();

        if (pid == 0) {
            printf("Forked new client with id %d\n", new_client->id);
            return serveClient(new_client);
        } else {
            closeClient(new_client);
        }
    }

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
    if (client->socket < 0) {
        printf("Connection Failed\n");
        return 1;
    }

    printf("Connection success\n");

    int connectionAlive = 5;
    char *recBuff = malloc(sizeof(char) * recvBuffSize);

    //while (connectionAlive --> 0) {
    printf("Waiting to receive\n");
    int buffLen = recieve(client, recBuff);
    printf("\nReceived\n%s\nparsing request... \n", recBuff);
    struct HTTP_request *request = parse_request(client, recBuff, buffLen);
    printf("Parsed\n");
    serve(client, request);
    free_request(request);
    //}
    free(recBuff);
    closeClient(client);

}

int recieve(struct Client *client, char *buff) {
    int bufferSize = sizeof(char) * recvBuffSize;
    recv(client->socket, buff, bufferSize, 0);
    int CRLF_pos;
    while ((CRLF_pos = strstr(buff, "\r\n")) == -1) {
        bufferSize += sizeof(char) * recvBuffSize;
        realloc(buff, bufferSize);
    }

    return CRLF_pos;
}

int closeClient(struct Client *client) {
    printf("Closing client %d... ");
    close(client->socket);
    free(client);
    printf("SUCCESS\n");
    return 0;
}