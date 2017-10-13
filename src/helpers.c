#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "helpers.h"
#include <dirent.h>

struct HTTP_request *parse_request(char *request, int buffLen) {
    struct HTTP_request *result = malloc(sizeof(struct HTTP_request));

    // What type of request is it?
    if (strncmp(request, "GET", 3) == 0) {
        result->requestMethod = GET;
    }

    // Extract the request URI
    char *requestURIStartPtr = strchr(request, ' ') + 1;
    char *requestURIEndPtr = strchr(requestURIStartPtr, ' ');
    int URILength = requestURIEndPtr - requestURIStartPtr;
    char *URI = malloc(sizeof(char) * (URILength + 1));
    strncpy(URI, requestURIStartPtr, URILength);
    URI[URILength] = '\0';
    result->requestURI = URI;
    printf("result->requestURI: %s\n", result->requestURI);

    // Extract the host
    char *host = extract_header_item(request, "Host");
    result->host = host;

    // Extract Referer
    char *referer = extract_header_item(request, "Referer");
    if (referer != NULL)
        result->referer = referer;

    // Extract fully qualified URI
    if (result->referer != NULL) {
        char *refererHostPtr = strstr(result->referer, result->host);
        char *refererURI = refererHostPtr + strlen(result->host);
        char *FQrequestURI = malloc(sizeof(char) * (strlen(refererURI) + strlen(result->requestURI) + 1));
        strcpy(FQrequestURI, refererURI);
        strcat(FQrequestURI, result->requestURI);
        result->FQrequestURI = FQrequestURI;
    } else {
        result-> FQrequestURI = result->requestURI;
    }

    return result;
}

// TODO: Needs a better check for header not existing
char *extract_header_item(char *request, char *identifier) {

    char *headerItemPtr = strstr(request, identifier);
    if (headerItemPtr == NULL)
        return NULL;

    char *valuePtr = headerItemPtr + strlen(identifier) + 2;
    char *valueEndPtr = strstr(valuePtr, "\n");
    int valueLen = valueEndPtr - valuePtr;

    char *value = malloc(sizeof(char) * (valueLen + 1));
    memcpy(value, valuePtr, valueLen);

    return value;
}

int free_request(struct HTTP_request *request) {
    // TODO: Needs checks for invalid free
    free(request->host);
    free(request->content);
    free(request->referer);
    free(request->requestURI);
    free(request->FQrequestURI);
    free(request);
    return 0;
}

int free_response(struct HTTP_response *response) {
    //TODO: Needs check for invalid free
    free(response->content);
    free(response->reasonPhrase);
    free(response->statusCode);
    free(response);
    return 0;
}


struct HTTP_response *directoryListing(struct HTTP_request *request) {
    struct HTTP_response *response = malloc(sizeof(struct HTTP_response));

    int length = 100;
    char *responseHTML = malloc(sizeof(char) * length);
    strcpy(responseHTML, "<html><body>");

    char dir[1024];

    strcpy(dir, "/home/george");

    strcat(dir, request->requestURI);

    DIR *dp;
    dp = opendir(dir);

    struct dirent *ep;

    if (dp != NULL) {
        while (ep = readdir(dp)) {

            char line[2048] = "";
            strcat(line, "<a href='");
            strcat(line, request->requestURI);
            strcat(line, "/");
            strcat(line, ep->d_name);
            strcat(line, "'>");
            strcat(line, ep->d_name);
            strcat(line, "</a><br />");
            int newLen = strlen(responseHTML) + strlen(line) + 1;
            if (newLen > length) {
                length = newLen;
                responseHTML = realloc(responseHTML, sizeof(char) * length);
            }
            strcat(responseHTML, line);
        }
        printf("Closing dir\n");
        closedir(dp);
    } else {
        perror("Couldn't open directory");
        return error_404();
    }

    printf("Finished generating HTML\n");
    strcat(responseHTML, "</body></html>");

    response->content = responseHTML;
    response->statusCode = "200";
    response->reasonPhrase = "OK";
    printf("%d: Should return!\n", getpid());
    return response;
}

struct HTTP_response *file(struct HTTP_request *request) {
    struct HTTP_response *response;

    /**
     * Serve a file to the client
     */

    return response;
}

struct HTTP_response *error_404(){
    struct HTTP_response *response = malloc(sizeof(response));
    response->statusCode = "404";
    response->reasonPhrase = "Not Found";
    response->content = "The requested resource could not be located";
}