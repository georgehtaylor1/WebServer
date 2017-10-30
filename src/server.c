#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "helpers.h"
#include "server.h"

#define RECVBUFFSIZE 1024
#define CONNECTION_TIMEOUT 30000
#define CONNECTION_REQUEST_LIMIT 10

struct Client *new_client = NULL;
int sock_desc;

int connectionAlive, serverAlive;

/**
 * Handle a SIGINT interrupt.
 * @param signo The signal sent to the program.
 */
void sig_handler(int signo) {
    if (signo == SIGINT)
        printf("Killing server\n");

    connectionAlive = 0;
    serverAlive = 0;

    printf("Process terminated\n");

}

/**
 * Produce an error.
 * @param msg THe message for the error.
 * @return return -1 as a standard.
 */
int error(char *msg) {
    perror(msg);
    return -1;
}

/**
 * Start the server.
 * @param argc How many arguments have been passed to the process.
 * @param argv THe arguments for the process.
 * @return 0 on success, -1 on failure.
 */
int main(int argc, char **argv) {

    // Check if sufficient parameters have been supplied to the program.
    if (argc < 2) {
        error("Insufficient paramters supplied, usage: ./server <-d?> <port> <web directory>");
        exit(EXIT_FAILURE);
    }

    // Set up the program to catch SIGINT
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        error("Can't catch SIGINT");
        exit(EXIT_FAILURE);
    }

    // Should the program be run as a daemon?
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

    // Get the port number from the parameters and parse the root directory
    int port = atoi(argv[1]);
    server_root_dir = argv[2];

    // Remove the trailing '/' from the root directory if there is one
    if (server_root_dir[strlen(server_root_dir) - 1] == '/')
        server_root_dir[strlen(server_root_dir) - 1] = '\0';

    // Create the socket
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

    // Bind the socket to the port
    if (bind(sock_desc, (struct sockaddr *) &server, sizeof(server)) == -1) {
        error("Failed to bind");
        exit(EXIT_FAILURE);
    }

    // Start listening on the socket
    if (listen(sock_desc, 2) == -1) {
        error("Failed to listen on socket");
        exit(EXIT_FAILURE);
    }

    serverAlive = 1;
    int clientCounter = 0;

    // Set up the poller
    struct pollfd ufds[1];
    ufds[0].fd = sock_desc;
    ufds[0].events = POLLIN;

    while (serverAlive == 1) {
        // Connect a new client
        new_client = malloc(sizeof(struct Client));
        if (new_client == NULL) {
            perror("Failed to allocate memory for new client");
        }

        // Poll for a waiting client
        int pollVal = poll(ufds, 1, -1);
        if (pollVal > 0) {

            //Accept a new client
            new_client->socket = accept(sock_desc, &new_client->sock_addr, &new_client->client_length);
            if (new_client->socket != -1) {
                new_client->id = clientCounter++;

                // Fork a new process to handle the client
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
 * Serve a particular client while ever it's connection is alive.
 * @param client The client to be served.
 * @return Did the client serving complete correctly.
 */
int serveClient(struct Client *client) {
    printf("%d: Serving client %d\n", getpid(), client->id);

    if (client->socket < 0) return error("Connection failed");

    printf("Connection success\n");

    connectionAlive = CONNECTION_REQUEST_LIMIT;

    // While the connection hasn't expired
    while (connectionAlive > 0) {

        // Create the buffer to receive a request
        char *recBuff = malloc(sizeof(char) * RECVBUFFSIZE);
        if (recBuff == NULL)
            return error("Failed to allocate memory for buffer");

        // Create any additional headers to be sent to this client
        char headers[2048];
        strcpy(headers, "");
        strcat(headers, (connectionAlive > 1) ? "Connection: keep-alive\n" : "Connection: close\n");
        strcat(headers, "Allowed: GET, HEAD\n");
        strcat(headers, "Content-Language: en\n");
        strcat(headers, "Server: Ju5t a humbl3 s3rv3r\n");
        strcat(headers, "Date: ");
        char date[29];
        create_date(date);
        strcat(headers, date);
        strcat(headers, "\n");

        printf("Waiting to receive\n");

        // Setup and poll the socket for a new request, timing out if no request arrives
        struct pollfd ufds[1];
        ufds[0].fd = client->socket;
        ufds[0].events = POLLIN;
        int pollVal = poll(ufds, 1, CONNECTION_TIMEOUT);

        // If the poll indicates a message was received
        if (pollVal > 0) {

            // Pull the request off the socket
            int buffLen = receive(client, &recBuff);
            if (buffLen != 0) {

                // Create the request object
                printf("\nReceived\n%s\nparsing request... \n", recBuff);
                struct HTTP_request *request = parse_request(client, recBuff, headers);
                if (request != NULL) {

                    if (request->keep_alive != 1)
                        connectionAlive = 0;

                    // Server the response to this particular request to the client
                    serve(client, request, headers);

                    free_request(request);
                    connectionAlive--;
                }
            } else {
                // No data was received for the client so close it and free the request buffer
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
 * Pull a single request off the socket.
 * @param client THe client to receive the request from.
 * @param buff The buffer to read the request into.
 * @return The length of the buffer.
 */
int receive(struct Client *client, char **buff) {

    // Setup the poll
    struct pollfd ufds[1];
    ufds[0].fd = client->socket;
    ufds[0].events = POLLIN;

    int bufferSize;
    int buffLen = 0;
    int numBytes = RECVBUFFSIZE;

    // Poll until a request is received by the socket
    int pollVal = poll(ufds, 1, -1);

    // Repeatedly pull RECVBUFFSIZE chunks off the socket
    while (numBytes == RECVBUFFSIZE) {
        numBytes = recv(client->socket, *buff + buffLen, RECVBUFFSIZE, 0);
        if (numBytes == -1) {
            perror("Error recv()ing bytes");
            return 0;
        }

        // Put the bytes received onto the request buffer and expand the buffer to fit more bytes
        buffLen += numBytes;
        bufferSize = buffLen + RECVBUFFSIZE;
        if ((*buff = realloc(*buff, bufferSize)) == NULL) {
            perror("Failed to realloc for receiving request");
            return 0;
        }
    }
    if (pollVal == -1 || (pollVal == 0 && buffLen == 0))
        return 0;

    // Terminate the buffer correctly
    *(*buff + buffLen) = '\0';

    return buffLen;

}

/**
 * Create a string representation of the date
 * @param date The date buffer to be filled, must be 29 characters
 */
void create_date(char *date) {

    // Get the current time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    //@formatter:off
    // Create the three character day
    char *day;
    switch(tm.tm_wday){
        case 0: day = "Sun"; break;
        case 1: day = "Mon"; break;
        case 2: day = "Tue"; break;
        case 3: day = "Wed"; break;
        case 4: day = "Thu"; break;
        case 5: day = "Fri"; break;
        case 6: day = "Sat"; break;
    }

    // Create the three character month
    char *mon;
    switch(tm.tm_mon){
        case 0: mon = "Jan"; break;
        case 1: mon = "Feb"; break;
        case 2: mon = "Mar"; break;
        case 3: mon = "Apr"; break;
        case 4: mon = "May"; break;
        case 5: mon = "Jun"; break;
        case 6: mon = "Jul"; break;
        case 7: mon = "Aug"; break;
        case 8: mon = "Sep"; break;
        case 9: mon = "Oct"; break;
        case 10: mon = "Nov"; break;
        case 11: mon = "Dec"; break;
    }
    //@formatter:on
    sprintf(date, "%s, %02d %s %04d %02d:%02d:%02d GMT", day, tm.tm_mday, mon, tm.tm_year + 1900, tm.tm_hour, tm.tm_min,
            tm.tm_sec);
}

/**
 * Close a connection to a client
 * @param client The client to close the connection to
 * @return 0 on success, -1 on error
 */
int closeClient(struct Client *client) {

    if (client == NULL) return error("Failed to close uninitialised client");

    // Close the client and free any space it was consuming
    printf("%d - Closing client %d...\n", getpid(), client->id);
    close(client->socket);
    free(client);
    return 0;
}