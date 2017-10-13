enum RequestMethod {POST, GET, HEAD, PUT, DELETE, OPTIONS, CONNECT};

struct HTTP_request {
    enum RequestMethod requestMethod;
    char* requestURI;
    char* host;
    char* referer;
    char* content;
};

struct HTTP_response {
    char* statusCode;
    char* reasonPhrase;
    char* content;
    int contentLength;
};

struct HTTP_request * parse_request(char *request, int i);

struct HTTP_response * directoryListing(struct HTTP_request *);

struct HTTP_response * file(struct HTTP_request *);