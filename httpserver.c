#include <stdio.h>       // printf, perror
#include <stdlib.h>      // malloc, free, exit
#include <string.h>      // strlen, strcpy, strtok
#include <unistd.h>      // read, write, close
#include <arpa/inet.h>   // sockaddr_in, htons, INADDR_ANY
#include <sys/socket.h>  // socket, bind, listen, accept
#include <pthread.h>     // pthread_create, pthread_detach
#include <stdbool.h>	 // boolean





struct client_info {
	int filedesc;
	struct sockaddr_in address;
};

void *thread_run(void* arg){
	struct client_info *c = (struct client_info*)arg;
	char ip[INET_ADDRSTRLEN]; // makes a string for the ip, based on how long it is
	inet_ntop(AF_INET, &c->address.sin_addr,ip,INET_ADDRSTRLEN); // converts to readable ip, needs protocol, pointer to the addr value, string to store it in and length.
	
	printf("successful connection from %s\n",ip);
	close(c->filedesc);
	free(c);
	return NULL;
}






int main (){
	int port = 8081;
	//creating the socket
	int server = socket(AF_INET, SOCK_STREAM, 0);
	if (server<0){
		perror("socket not created.\n");
		return 1;
	}
	// creating the listening socket
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET; // stores the address family
	server_addr.sin_addr.s_addr = INADDR_ANY; // stores the ip. sin_addr is a struct member in the addr struct, it is the name of an in_addr struct, which holds the ip at s_addr.
	server_addr.sin_port = htons(port);

	// binding the socket
	if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){ //bind takes in the socket made, a pointer to the socket structure we made, but being cast as a more generic type of sockaddr
		close(server);
		perror("socket couldnt bind.\n");
		return 1;
		
	}

	//listening
	if (listen(server, 5)<0){ // tries to listen, if theres an error it returns -1
		close(server);
		perror("couldnt listen.\n");
		return 1;
	}
	printf("server is listening on port %d.\n",port);

	//loop for accepting clients
	while(true){
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client = accept(server, (struct sockaddr*)&client_addr, &client_len);
		if (client<0){
			perror("couldnt accept client.\n");
			//not returning 1, because the loop should just continue
		}
		struct client_info {
			int filedesc;
			struct sockaddr_in address;
		};
		//allocating memory for the client
		struct client_info *c = malloc(sizeof(struct client_info));
		c->filedesc = client;
		c->address = client_addr;

		//threads
		pthread_t thread;
		pthread_create(&thread, NULL, thread_run, c);
		pthread_detach(thread);



	}
}

