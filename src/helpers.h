enum RequestMethod {POST, GET, HEAD, PUT, DELETE, OPTIONS, CONNECT};

struct HTTP_request {
    enum RequestMethod requestMethod;
    char* requestURI;
    char* FQrequestURI;
    char* host;
    char* referer;
    char* content;
};

struct HTTP_response {
    char* statusCode;
    char* reasonPhrase;
    char* content;
};

struct HTTP_request * parse_request(char *request, int i);

char * extract_header_item(char *request, char *identifier);

int serve_file(struct Client *client, struct HTTP_request *request);

int serve_directory_listing(struct Client *client, struct HTTP_request *request);

struct HTTP_response * file(struct HTTP_request *);

// Error functions
struct HTTP_response *error_404();