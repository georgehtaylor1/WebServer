#include <sys/socket.h>

struct Client {
    int id;
    int socket;
    struct sockaddr sock_addr;
    int client_length;
};

int recieve(struct Client *client, char *buff);

int closeClient(struct Client *client);