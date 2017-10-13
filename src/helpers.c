#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "server.h"
#include "helpers.h"
#include <dirent.h>
#include <sys/stat.h>

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

int serve_file(struct Client *client, struct HTTP_request * request){

    char dir[1024];
    strcpy(dir, "/home/george");
    strcat(dir, request->requestURI);
    printf("Dir: %s\n", dir);
    if(is_directory(dir) != 0){
        return redirect(client, "index.html");
    }

    if(file_exists(dir)) {
        // Serve file
        perror("File found");
    } else {
        int dirLen = strlen(dir);
        printf("Last ten chars: %s\n", dir + dirLen - 10);
        if(strcmp("index.html", dir + dirLen - 10) == 0)
            serve_directory_listing(client, request);
        else
            perror("Something funny occurred...");
    }
}

int serve_directory_listing(struct Client *client, struct HTTP_request *request) {
    struct HTTP_response *response = malloc(sizeof(struct HTTP_response));

    int length = 100;
    char *responseHTML = malloc(sizeof(char) * length);
    strcpy(responseHTML, "<html><body>");

    char dir[1024];

    strcpy(dir, "/home/george");

    strcat(dir, request->requestURI);
    int dirLen = strlen(dir);

    if(strcmp("index.html", dir + dirLen - 10) == 0) {
        dir[dirLen - 10] = '\0';
        printf("Stripped dir: %s\n", dir);
    }

    DIR *dp;
    dp = opendir(dir);

    struct dirent *ep;

    if (dp != NULL) {
        while (ep = readdir(dp)) {
            // TODO: Fix this to use sprintf
            char line[2048] = "";

            char path[1024], FQpath[1024];
            sprintf(path, "%s/%s", request->requestURI, ep->d_name);
            sprintf(FQpath, "/home/george%s", path);
            printf("Adding path: %s\n", path);
            strcat(line, "<a href='");
            strcat(line, ep->d_name);
            if(is_file(FQpath) == 0){
                strcat(line, "/index.html");
            }
            strcat(line, "'>");
            strcat(line, ep->d_name);
            if(is_file(FQpath) == 0){
                strcat(line, "/");
            }
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

    write(client->socket, "HTTP/1.1 200 OK\n\n", 17);
    write(client->socket, responseHTML, strlen(responseHTML) + 1);

//    response->content = responseHTML;
//    response->statusCode = "200";
//    response->reasonPhrase = "OK";
    printf("%d: Should return!\n", getpid());
    return response;
}

int is_file(char *path){
    struct stat pathStat;
    stat(path, &pathStat);
    return S_ISREG(pathStat.st_mode);
}

int is_directory(char *path){
    struct stat pathStat;
    stat(path, &pathStat);
    return S_ISDIR(pathStat.st_mode);
}

int file_exists(char *path){
    struct stat pathStat;
    return stat(path, &pathStat) == 0;
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

int redirect(struct Client *client, char *red){
    printf("Redirected\n");
    write(client->socket, "HTTP/1.1 301 Moved Permanently\n\n", 32);
    write(client->socket, "Location: ", 10);
    write(client->socket, red, strlen(red) + 1);
    write(client->socket, "\n\n", 2);
}