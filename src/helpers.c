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

struct HTTP_request *parse_request(struct Client *client, char *request, int buffLen, char *headers) {
    struct HTTP_request *result = malloc(sizeof(struct HTTP_request));

    // What type of request is it?
    if (strncmp(request, "GET", 3) == 0) {
        result->requestMethod = GET;
    } else {
        response_501(client, headers);
        return NULL;
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

    // Extract keep-alive
    char *keep_alive = extract_header_item(request, "Connection");
    printf("%s\n", keep_alive);
    if (keep_alive != NULL) {
        result->keep_alive = (strncmp(keep_alive, "keep-alive", 10) == 0) ? 1 : 0;
        printf("strncmp = %d\n", strncmp(keep_alive, "keep-alive", 10));
        free(keep_alive);
    }

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
    free(request->referer);
    free(request->requestURI);
    free(request);
    return 0;
}

int serve(struct Client *client, struct HTTP_request *request, char *headers) {

    char dir[1024];
    strcpy(dir, server_root_dir);
    strcat(dir, request->requestURI);
    printf("Dir: %s\n", dir);

    if (is_file(dir) == 0)
        return serve_directory_listing(client, request, headers);

    if (file_exists(dir))
        return serve_file(client, dir, headers);
    else
        return response_404(client, headers);
}

int serve_directory_listing(struct Client *client, struct HTTP_request *request, char *headers) {

    int length = 100;
    char *responseHTML = malloc(sizeof(char) * length);
    strcpy(responseHTML, "<html><body>");

    char dir[PATH_MAX];

    strcpy(dir, server_root_dir);

    strcat(dir, request->requestURI);

    printf("dir: %s\n", dir);

    DIR *dp;
    dp = opendir(dir);

    struct dirent *ep;

    if (dp != NULL) {
        while ((ep = readdir(dp)) != NULL) {
            // TODO: Fix this to use sprintf
            char line[PATH_MAX + 24] = "";
            char path[PATH_MAX];

            sprintf(path, "%s%s", dir, ep->d_name);

            strcat(line, "<a href='");
            strcat(line, ep->d_name);

            if (is_file(path) == 0)
                strcat(line, "/");

            strcat(line, "'>");
            strcat(line, ep->d_name);

            if (is_file(path) == 0)
                strcat(line, "/");

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
        if (errno == ENOENT) {
            perror("Not a valid directory");
            if(responseHTML != NULL) free(responseHTML);
            return response_404(client, headers);
        }
        if (errno == EACCES) {
            perror("Illegal file access");
            if(responseHTML != NULL) free(responseHTML);
            return response_403(client, headers);
        }
        perror("Couldn't open directory");
        if(responseHTML != NULL) free(responseHTML);
        return response_500(client, headers);
    }

    responseHTML = realloc(responseHTML, sizeof(char) * (strlen(responseHTML) + 16));
    strcat(responseHTML, "</body></html>");

    write(client->socket, "HTTP/1.1 200 OK\n", 16);
    write(client->socket, headers, strlen(headers));

    char contentLength[128];
    sprintf(contentLength, "Content-length: %zu\n\n", strlen(responseHTML));

    write(client->socket, contentLength, strlen(contentLength));
    write(client->socket, responseHTML, strlen(responseHTML) + 1);
    free(responseHTML);
    return 0;
}

int serve_file(struct Client *client, char file[1024], char *headers) {

    FILE *f = fopen(file, "r");
    if (f == NULL) {
        perror("Failed to open file");
        return response_500(client, NULL);
    }

    fseek(f, 0, SEEK_END);
    int length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char contentLength[1024];
    sprintf(contentLength, "Content-length: %d", length);

    write(client->socket, "HTTP/1.1 200 OK\n", 17);
    write(client->socket, headers, strlen(headers));
    write(client->socket, contentLength, strlen(contentLength));
    write(client->socket, "\n\n", 2);

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

/**
 * Check if a path specifies a file, directory or other
 * @param name The path to be checked
 * @return 0 if <name> is a directory, 1 if it is a file, -1 in any other case
 */
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

int response_404(struct Client *client, char *headers) {
    write(client->socket, "HTTP/1.1 404 Not Found\n", 23);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    write(client->socket, "The requested resource could not be located", 43);
    return 0;
}

int response_403(struct Client *client, char *headers) {
    write(client->socket, "HTTP/1.1 403 Forbidden\n", 23);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    write(client->socket, "The user is not authorized to access this service", 50);
    return 0;
}

int response_500(struct Client *client, char *headers) {
    write(client->socket, "HTTP/1.1 500 Internal Server Error\n", 37);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    return 1;
}

int response_501(struct Client *client, char *headers) {
    write(client->socket, "HTTP/1.1 501 Not Implemented\n", 29);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    return 1;
}
//
//int temp_redirect(struct Client *client, char *red) {
//    printf("Redirected\n");
//    write(client->socket, "HTTP/1.1 302 Found\n", 19);
//    write(client->socket, headers, strlen(headers));
//    write(client->socket, "Location: ", 10);
//    write(client->socket, red, strlen(red) + 1);
//    write(client->socket, "\n\n", 2);
//    return 0;
//}