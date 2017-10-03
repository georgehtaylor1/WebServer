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