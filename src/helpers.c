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

/**
 * Parse a plaintext HTTP request.
 * @param client The client that sent the request.
 * @param request The plaintext request sent by the client.
 * @param headers Any headers hat should be sent in the response to the client. A response is only sent in the case that an error occurs.
 * @return Returns a NULL pointer in the case that the response could not be parsed or was not accepted. Returns a pointer to the request structure that was generated by the method on success.
 */
struct HTTP_request *parse_request(struct Client *client, char *request, char *headers) {
    struct HTTP_request *result = malloc(sizeof(struct HTTP_request));

    // What type of request is it?
    if (strncmp(request, "GET", 3) == 0) {
        result->requestMethod = GET;
    } else if (strncmp(request, "HEAD", 4) == 0) {
        result->requestMethod = HEAD;
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

/**
 * Extract the value of the specified header item from the HTTP request.
 * @param request The HTTP request containing the header.
 * @param identifier The identifier of the header value to be extracted.
 * @return The value extracted from the specified header item.
 */
char *extract_header_item(char *request, char *identifier) {

    // Get a pointer to the start of the requested header item
    char *headerItemPtr = strstr(request, identifier);
    if (headerItemPtr == NULL)
        return NULL;

    // Calculate the pointer to the start of the value by adding the length of the
    // header identifier to the start of the header item, and two more bytes for
    // the ': '
    char *valuePtr = headerItemPtr + strlen(identifier) + 2;
    char *valueEndPtr = strstr(valuePtr, "\n");
    int valueLen = valueEndPtr - valuePtr;

    char *value = malloc(sizeof(char) * (valueLen + 1));
    memcpy(value, valuePtr, valueLen);

    return value;
}

/**
 * Free the memory allocated to a request object.
 * @param request The request object to be freed.
 */
void free_request(struct HTTP_request *request) {
    free(request->host);
    free(request->referer);
    free(request->requestURI);
    free(request);
}

/**
 * Serve either a directory listing or file to the specified client. Respond with a 404 error if the resource can not be located.
 * @param client The client to whom the response should be served.
 * @param request The HTTP request sent by the client.
 * @param headers Any headers that should be sent in the response.
 * @return returns 0 on success or -1 on failure. If an error occurs but an appropriate response is still sent to the client then this is seen as a success.
 */
int serve(struct Client *client, struct HTTP_request *request, char *headers) {

    char dir[1024];
    strcpy(dir, server_root_dir);
    strcat(dir, request->requestURI);
    printf("Dir: %s\n", dir);

    // If the requested resource is a directory then return a directory listing
    if (is_file(dir) == 0)
        return serve_directory_listing(client, request, headers);

    // If the file exists then serve the file, otherwise return a 404
    if (file_exists(dir))
        return serve_file(client, request, dir, headers);
    else
        return response_404(client, request, headers);
}

/**
 * Produce a directory listing and send it to the client.
 * @param client The client to whom the directory listing should be sent.
 * @param request The original HTTP request sent by the client.
 * @param headers Any additional headers that should be sent to the client.
 * @return -1 on failure. 0 on success. If an error occurs and is handled as a response successfully sent to the client then this is also seen as a success.
 */
int serve_directory_listing(struct Client *client, struct HTTP_request *request, char *headers) {

    // Server the response to the client
    write(client->socket, "HTTP/1.1 200 OK\n", 16);
    write(client->socket, "Content-type: text/html\n", 24);
    write(client->socket, headers, strlen(headers));

    if (request->requestMethod == GET) {

        // Create the object that will contain the response HTML
        int length = 100;
        char *responseHTML = malloc(sizeof(char) * length);
        strcpy(responseHTML, "<html><body>");

        char dir[PATH_MAX];
        strcpy(dir, server_root_dir);
        strcat(dir, request->requestURI);

        printf("dir: %s\n", dir);

        // Open the directory
        DIR *dp;
        dp = opendir(dir);

        struct dirent *ep;

        if (dp != NULL) {

            // For every item in the directory add a link to the response html taking the user to that resource
            while ((ep = readdir(dp)) != NULL) {
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

                // If the HTML buffer is no longer large enough then realloc it with more space
                int newLen = strlen(responseHTML) + strlen(line) + 1;
                if (newLen > length) {
                    length = newLen;
                    responseHTML = realloc(responseHTML, sizeof(char) * length);
                }
                strcat(responseHTML, line);
            }
            closedir(dp);
        } else {
            // Handle which error occurred
            if (errno == ENOENT) {
                perror("Not a valid directory");
                if (responseHTML != NULL) free(responseHTML);
                return response_404(client, request, headers);
            }
            if (errno == EACCES) {
                perror("Illegal file access");
                if (responseHTML != NULL) free(responseHTML);
                return response_403(client, request, headers);
            }
            perror("Couldn't open directory");
            if (responseHTML != NULL) free(responseHTML);
            return response_500(client, headers);
        }

        responseHTML = realloc(responseHTML, sizeof(char) * (strlen(responseHTML) + 16));
        strcat(responseHTML, "</body></html>");

        char contentLength[128];
        sprintf(contentLength, "Content-length: %zu\n\n", strlen(responseHTML));

        write(client->socket, contentLength, strlen(contentLength));
        write(client->socket, responseHTML, strlen(responseHTML) + 1);
        free(responseHTML);
    } else {
        write(client->socket, "\n\n", 2);
    }
    return 0;
}

/**
 * Serve the requested file to the client.
 * @param client The client that requested the file.
 * @param request The original request sent by the client.
 * @param file THe URI of the file that has been requested.
 * @param headers Any additional headers to be sent to the client.
 * @return -1 on failure, 0 on success. If an error occurs but it is handled suitably by the server then this is considered a success.
 */
int serve_file(struct Client *client, struct HTTP_request *request, char file[1024], char *headers) {

    // Open the requested file
    FILE *f = fopen(file, "r");
    if (f == NULL) {
        perror("Failed to open file");
        return response_500(client, NULL);
    }

    // Get the length of the file
    fseek(f, 0, SEEK_END);
    int length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char contentLength[1024];
    sprintf(contentLength, "Content-length: %d\n", length);

    // Send the response header to the client
    write(client->socket, "HTTP/1.1 200 OK\n", 17);
    write(client->socket, headers, strlen(headers));
    write(client->socket, contentLength, strlen(contentLength));

    // Extract the file extension and determine the appropriate content type from this
    char *ext = strchr(file, '.');
    if (ext != NULL && ext != file) {
        char *content_type = get_content_type(ext);
        if (content_type != NULL) {
            char content_out[1024];
            sprintf(content_out, "Content-type: %s\n", content_type);
            write(client->socket, content_out, strlen(content_out));
        }
    }

    write(client->socket, "\n\n", 2);

    if (request->requestMethod == GET) {
        char buffer[MAX_READ_BUFFER];
        int bytes_read = 0;
        char *bufferPtr;

        // While there are still bytes being read from the file send them to the client across the socket.
        // This will occur in chunks of MAX_READ_BUFFER size
        while ((bytes_read = fread(buffer, sizeof(char), MAX_READ_BUFFER, f)) > 0) {
            int bytes_written = 0;
            bufferPtr = buffer;
            // While there are still bytes to be sent on the socket keep streaming them
            while (bytes_written < bytes_read) {
                bytes_read -= bytes_written;
                bufferPtr += bytes_written;
                bytes_written = write(client->socket, bufferPtr, bytes_read);
            }
        }
        fclose(f);

        printf("Successfully sent %s\n", file);
    }
    return 0;
}

/**
 * Determine the content type from the given file extension.
 * @param extension The file extension to be checked.
 * @return The string content type for the extension or NULL if no content type was found.
 */
char *get_content_type(char *extension) {
    if (strcmp(extension, ".pdf") == 0) return "application/pdf";
    if (strcmp(extension, ".js") == 0) return "application/javascript";
    if (strcmp(extension, ".xml") == 0) return "application/xml";
    if (strcmp(extension, ".zip") == 0) return "application/zip";
    if (strcmp(extension, ".html") == 0) return "text/html";
    if (strcmp(extension, ".txt") == 0) return "text/plain";
    if (strcmp(extension, ".css") == 0) return "text/css";
    if (strcmp(extension, ".csv") == 0) return "text/csv";
    if (strcmp(extension, ".png") == 0) return "image/png";
    if (strcmp(extension, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(extension, ".gif") == 0) return "image/gif";
    return NULL;
}

/**
 * Check if a path specifies a file, directory or other.
 * @param name The path to be checked.
 * @return 0 if <name> is a directory, 1 if it is a file, -1 in any other case.
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

/**
 * Determine if the specified file exists.
 * @param path The file to be checked.
 * @return 1 if the file exists, 0 otherwise
 */
int file_exists(char *path) {
    struct stat pathStat;
    return stat(path, &pathStat) == 0;
}

/**
 * Serve a 404 response to the client.
 * @param client The client to send to.
 * @param request The original request sent by the client.
 * @param headers Any additional headers to be sent to the client.
 * @return 0 on success.
 */
int response_404(struct Client *client, struct HTTP_request *request, char *headers) {
    write(client->socket, "HTTP/1.1 404 Not Found\n", 23);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    if (request->requestMethod == GET) write(client->socket, "The requested resource could not be located", 43);
    return 0;
}

/**
 * Serve a 403 response to the client.
 * @param client The client to send to.
 * @param request The original request sent by the client.
 * @param headers Any additional headers to be sent to the client.
 * @return 0 on success.
 */
int response_403(struct Client *client, struct HTTP_request *request, char *headers) {
    write(client->socket, "HTTP/1.1 403 Forbidden\n", 23);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    if (request->requestMethod == GET) write(client->socket, "The user is not authorized to access this service", 50);
    return 0;
}

/**
 * Serve a 500 response to the client.
 * @param client The client to send to.
 * @param headers Any additional headers to be sent to the client.
 * @return 0 on success.
 */
int response_500(struct Client *client, char *headers) {
    write(client->socket, "HTTP/1.1 500 Internal Server Error\n", 37);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    return 0;
}

/**
 * Serve a 501 response to the client.
 * @param client The client to send to.
 * @param headers Any additional headers to be sent to the client.
 * @return 0 on success.
 */
int response_501(struct Client *client, char *headers) {
    write(client->socket, "HTTP/1.1 501 Not Implemented\n", 29);
    if (headers != NULL) write(client->socket, headers, strlen(headers));
    write(client->socket, "\n\n", 2);
    return 0;
}