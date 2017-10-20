#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "server.h"
#include "helpers.h"


#define MAX_READ_BUFFER 2048

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
    if (host != NULL)
        result->host = host;

    // Extract Referer
    char *referer = extract_header_item(request, "Referer");
    if (referer != NULL)
        result->referer = referer;

    return result;
}

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

int serve(struct Client *client, struct HTTP_request *request) {

    char dir[1024];
    strcpy(dir, server_root_dir);
    strcat(dir, request->requestURI);
    printf("Dir: %s\n", dir);
    if (is_file(dir) == 0) {
        return temp_redirect(client, "index.html");
    }

    if (file_exists(dir)) {
        // Serve file
        serve_file(client, dir);
    } else {
        int dirLen = strlen(dir);
        if (strcmp("index.html", dir + dirLen - 10) == 0)
            serve_directory_listing(client, request);
        else
            return error_404(client);
    }
    return 0;
}

int serve_directory_listing(struct Client *client, struct HTTP_request *request) {

    int length = 100;
    char *responseHTML = malloc(sizeof(char) * length);
    strcpy(responseHTML, "<html><body>");

    char dir[1024];

    strcpy(dir, server_root_dir);

    strcat(dir, request->requestURI);
    int dirLen = strlen(dir);

    if (strcmp("index.html", dir + dirLen - 10) == 0) {
        dir[dirLen - 10] = '\0';
    }

    DIR *dp;
    dp = opendir(dir);

    struct dirent *ep;

    if (dp != NULL) {
        while (ep = readdir(dp)) {
            // TODO: Fix this to use sprintf
            char line[2048] = "";

            char path[1024];

            sprintf(path, "%s%s", dir, ep->d_name);

            strcat(line, "<a href='");
            strcat(line, ep->d_name);
            if (is_file(path) == 0) {
                strcat(line, "/index.html");
            }
            strcat(line, "'>");
            strcat(line, ep->d_name);
            if (is_file(path) == 0) {
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
        closedir(dp);
    } else {
        perror("Couldn't open directory");
        return internal_server_error(client);
    }

    strcat(responseHTML, "</body></html>");

    write(client->socket, "HTTP/1.1 200 OK\n\n", 17);
    write(client->socket, responseHTML, strlen(responseHTML) + 1);

    return 0;
}

int serve_file(struct Client *client, char *file) {
    write(client->socket, "HTTP/1.1 200 OK\n", 17);
    write(client->socket, "\n\n", 2);

    FILE *f = fopen(file, "r");
    if (f < 0) {
        perror("Failed to open file");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    int length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char buffer[MAX_READ_BUFFER];
    int bytes_read = 0;
    char *bufferPtr;

    while ((bytes_read = fread(buffer, sizeof(char), MAX_READ_BUFFER, f)) > 0) {
        int bytes_written = 0;
        bufferPtr = buffer;
        while (bytes_written < bytes_read) {
            bytes_read -= bytes_written;
            bufferPtr += bytes_written;
            bytes_written = write(client->socket, bufferPtr, bytes_read);
        }
    }
    fclose(f);

    printf("Successfully sent %s\n", file);
    return 0;
}

int is_file(const char *name) {
    DIR *directory = opendir(name);

    if (directory != NULL) {
        closedir(directory);
        return 0;
    }

    if (errno == ENOTDIR)
        return 1;

    return -1;
}

int file_exists(char *path) {
    struct stat pathStat;
    return stat(path, &pathStat) == 0;
}

int error_404(struct Client *client) {
    write(client->socket, "HTTP/1.1 404 Not Found\n", 23);
    write(client->socket, "\n\n", 2);
    write(client->socket, "The requested resource could not be located", 43);
    return 1;
}

int internal_server_error(struct Client *client) {
    write(client->socket, "HTTP/1.1 500 Internal Server Error\n", 37);
    write(client->socket, "\n\n", 2);
    return 1;
}

int temp_redirect(struct Client *client, char *red) {
    printf("Redirected\n");
    write(client->socket, "HTTP/1.1 302 Found\n", 19);
    write(client->socket, "Location: ", 10);
    write(client->socket, red, strlen(red) + 1);
    write(client->socket, "\n\n", 2);
    return 0;
}