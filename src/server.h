#include <sys/socket.h>


struct Client {
    int id;
    int socket;
    struct sockaddr sock_addr;
    int client_length;
};


char * server_root_dir;

int recieve(struct Client *client, char *buff);

int serveClient(struct Client *client);

int serveRequest(struct Client *client, struct HTTP_request *request);

int closeClient(struct Client *client);