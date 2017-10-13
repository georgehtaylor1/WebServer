#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "helpers.h"
#include <dirent.h>

struct HTTP_request * parse_request(char *request, int buffLen) {
    struct HTTP_request *result = malloc(sizeof(struct HTTP_request));

    // What type of request is it?
    if (strncmp(request, "GET", 3) == 0){
        result->requestMethod = GET;
    }

    // Extract the request URI
    char* requestURIStartPtr = strchr(request, ' ') + 1;
    char* requestURIEndPtr = strchr(requestURIStartPtr, ' ');
    int URILength = requestURIEndPtr - requestURIStartPtr;
    char *URI = malloc(sizeof(char) * (URILength + 1))  ;
    strncpy(URI, requestURIStartPtr, URILength);
    URI[URILength] = '\0';
    result->requestURI = URI;
    printf("result->requestURI: %s\n", result->requestURI);

    return result;
}



struct HTTP_response *directoryListing(struct HTTP_request *request){
    struct HTTP_response *response = malloc(sizeof(struct HTTP_response));

    int length = 100;
    char *responseHTML = malloc(sizeof(char) * length);
    strcpy(responseHTML, "<html><body>");

    char dir[1024];

    strcpy(dir, "/home/george");
    strcat(dir, request->requestURI);

    DIR *dp;
//    printf("Opening directory\n");
    dp = opendir(dir);

    struct dirent *ep;

//    printf("Creating HTML\n");
    if(dp != NULL){
        while(ep = readdir(dp)){

//            printf("Adding: %s\n", ep->d_name);

            char line[2048] = "";
            strcat(line, "<a href='");
            strcat(line, ep->d_name);
            strcat(line, "'>");
            strcat(line, ep->d_name);
            strcat(line, "</a><br /><br />");
            int newLen = strlen(responseHTML) + strlen(line) + 1;
//            printf("Length: %d, Calculated newLen: %d\n", length, newLen);
            if (newLen > length) {
                length = newLen;
                responseHTML = realloc(responseHTML, sizeof(char) * length);
            }
            strcat(responseHTML, line);
//
//            printf("%s\n", responseHTML);
//            printf(" %d\n", strlen(responseHTML) + 1);
        }
        printf("Closing dir\n");
        closedir(dp);
    }else{
        perror("Couldn't open directory");
    }

    printf("Finished generating HTML\n");
    strcat(responseHTML, "</body></html>");

    response->content = responseHTML;
    response->statusCode = "200";
    response->reasonPhrase = "OK";
    printf("%d: Should return!\n", getpid());
    return response;
}

struct HTTP_response * file(struct HTTP_request *request){
    struct HTTP_response *response;

    /**
     * Serve a file to the client
     */

    return response;
}