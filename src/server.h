#include <sys/socket.h>


struct Client {
    int id;
    int socket;
    struct sockaddr sock_addr;
    socklen_t client_length;
};


char *server_root_dir;

int receive(struct Client *client, char **buff);

int serveClient(struct Client *client);

int closeClient(struct Client *client);

void create_date(char *date);