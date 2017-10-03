#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>





int main(int *argc, char **argv){

  int sock_desc = socket(AF_INET, SOCK_STREAM, 0);
  if(sock_desc == -1) { printf("Failed to create socket"); }
  
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(8080);


  if(bind(sock_desc, (struct sockaddr *) &server, sizeof(server)) == -1) { printf("Failed to bind\n"); }

  printf("Liistening\n");
  listen(sock_desc, 2);
  
  struct sockaddr_in client;
  int c = sizeof(struct sockaddr_in);
  int new_socket = accept(sock_desc, (struct sockaddr *)&client, (struct socklen_t *)&c);
  // Fork()

  if(new_socket < 0) { printf("Connection Failed\n"); }

  printf("Connection success\n");

  return 0;
  

}

