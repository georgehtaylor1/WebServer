# Notes

## 02/10/17

Researching socket programming in C

Settled on architecture:

 - Program starts & creates socket to start listening
 - When accept() create new fork()
   - If child then handle the new connection
   - If parent then start waiting for new client
 - When child finishes kill process

How do http requsts get served to the correct child thread? ephemeral ports?

Starting with socket preparatory 1


## 03/10/17

Successfully opening and closing socket (tested with telnet)

[15:08]

Su/ccessfully print get request

## 07/10/17

Setup structs for HTTP requests, responses and tcp clients.

Created framework for functions that produce a directory listing and for serving files

Settled on an implementation which uses fork() and a system timer + request count which allows connections to be kept
alive - Still need to work out how this will work when daemons are considered as it will require a host process which
spins off the child processes

## 13/10/17

The server will now examine the URI of a GET request and return a directory listing for that directory - this does not currently work for subdirectories as it needs to be combined with the referrer to determine the correct local path.

Reasonable temporary values are being used for the various buffers at all times. THey will be replaced by more rigourous values a thte end of the project.

[15:09]

Can now serve sub directories and returns error message in response when could not locate directory, needs updating to proper HTTP error. Next task is to serve files.

[23:04]

Performed a fairly major restructuring, if a directory is referenced by the URI it will return a redirect response to an index.html in that directory
If an index.html is referenced that doesn't exist then a directory listing page will be presented (This will probably end up being extended to referencing any file that doesn't exist in a directory)
This seems to be working fairly well but there is some funny business occuring when testing in google chrome. More debugging needed

## 17/10/17

Google chrome was playing up (unrelated) so now testing in firefox. Successfully serving directory listing except if the URI contains a space (%20).

## 19/10/17

The server now successfully serves files and handles errors in a far better way.
I've done a lot of tidying up of the code but there's more to be done.
Next task is to attempt some of the extensions and set the process up as a daemon.

## 22/10/17

With more time I'm glad to have the opportunity to make the solution far better!
The server has been cleaned up hugely and a lot of additional error checking has been introduced (although still not enough).
The receipt of messages now works by blocking until a poll() indicates that there are bytes to be read from the socket. The server will then read from the socket until a recv() request produces 0 bytes

## 23/10/17

A lot of cleaning up of the code has been done, and a lot of error checking has been added to server.c (needs repeating for the helpers)
Hopefully I'll be able to implement some of the extensions over the coming days!

## 28/10/17

Changed the architecture so that directory listings can be served without the need to redirect to an `index.html`
Fixed a pretty colossal memory leak. THe receiving buffer was being realloced but the pointer to the start was not being updated. This made the error appear non-deterministic as it would only occur if there was insufficient memory for an in-place realloc().
THe termination of the program has been tidied up and polling has been introduced to ensure the program can terminate immediately.

[19:53]

Added the ability to provide response headers and added keep-alive to the relevent responses. 