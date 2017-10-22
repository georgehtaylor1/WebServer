
enum RequestMethod {
    POST, GET, HEAD, PUT, DELETE, OPTIONS, CONNECT
};

struct HTTP_request {
    enum RequestMethod requestMethod;
    char *requestURI;
    char *FQrequestURI;
    char *host;
    char *referer;
    char *content;
    int keep_alive;
};

struct Client;

struct HTTP_request *parse_request(struct Client *client, char *request, int i);

char *extract_header_item(char *request, char *identifier);

int serve(struct Client *client, struct HTTP_request *request);

int serve_directory_listing(struct Client *client, struct HTTP_request *request);

int serve_file(struct Client *client, char *file);

int is_file(const char *name);

int file_exists(char *path);

int free_request(struct HTTP_request *request);

// Error functions
int error_404(struct Client *client);

int response_501(struct Client *client);

int temp_redirect(struct Client *client, char *red);

int response_500(struct Client *client);