#include <stdio.h>       // printf, perror
#include <stdlib.h>      // malloc, free, exit
#include <string.h>      // strlen, strcpy, strtok
#include <unistd.h>      // read, write, close
#include <arpa/inet.h>   // sockaddr_in, htons, INADDR_ANY
#include <sys/socket.h>  // socket, bind, listen, accept
#include <pthread.h>     // pthread_create, pthread_detach


struct client_info {
	int filedesc;
	struct sockaddr_in address;
};

struct thread_manager {
	int id;
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int has_work;
	struct client_info *client;
};

void *thread_run(void* arg){
	struct thread_manager *t = (struct thread_manager*)arg;
	printf("Hello from Thread %d!\n",t->id);
	while (1){
		pthread_mutex_lock(&t->mutex);
		while(!t->has_work){
			pthread_cond_wait(&t->cond,&t->mutex);
		}
		struct client_info *local = malloc(sizeof(struct client_info));
		*local = *t->client;
		char ip[INET_ADDRSTRLEN]; // makes a string for the ip, based on how long it is
        	inet_ntop(AF_INET, &local->address.sin_addr,ip,INET_ADDRSTRLEN); // converts to readable ip, needs protocol, pointer to the addr value, string to store it in and length

        	printf("Successful connection from %s...\n",ip);
		printf("Connection from %s is being handled by Thread %d.\n",ip,t->id);
		//http logic goes here
		//
		sleep(10); // temp simulating processing time








		//http logic goes above
		//
		printf("Thread %d has completed its work.\n",t->id);
        	close(local->filedesc);
		t->has_work = 0;
		t->client = NULL;
		pthread_mutex_unlock(&t->mutex);
        	free(local);
	}
	free(t);
	return NULL;
}


int main (){
	// variables
	int port = 8081;
	int tp_size=5;

	//creating the socket
	int server = socket(AF_INET, SOCK_STREAM, 0);
	if (server<0){
		perror("socket not created.\n");
		return 1;
	}

	// creating the listening socket
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET; // stores the address family
	server_addr.sin_addr.s_addr = INADDR_ANY; // stores the ip. sin_addr is a struct member in the addr struct, 
						  // it is the name of an in_addr struct, which holds the ip at s_addr.
	server_addr.sin_port = htons(port);

	// binding the socket
	if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){ //bind takes in the socket made, a pointer to the socket structure we made, 
										  //but being cast as a more generic type of sockaddr
		close(server);
		perror("socket couldnt bind.\n");
		return 1;
		
	}
	
	//set up threads
	struct thread_manager threadpool[tp_size];
	for(int i = 0;i<tp_size;i++){
		threadpool[i].id=i;
		threadpool[i].has_work=0;
		pthread_mutex_init(&threadpool[i].mutex,NULL);
		pthread_cond_init(&threadpool[i].cond, NULL);
		pthread_create(&threadpool[i].thread, NULL, thread_run, &threadpool[i]);

	}

	//listening
	if (listen(server, 5)<0){ // tries to listen, if theres an error it returns -1
		close(server);
		perror("couldnt listen.\n");
		return 1;
	}
	printf("server is listening on port %d.\n",port);

	//loop for accepting clients
	while(1){
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client = accept(server, (struct sockaddr*)&client_addr, &client_len);
		if (client<0){ // checking for success
			perror("couldnt accept client.\n");
			//not returning 1, because the loop should just continue
		}
		
		//allocating memory for the client
		struct client_info *c = malloc(sizeof(struct client_info));
		c->filedesc = client;
		c->address = client_addr;

		//managing resource assignment
		int found =0;
		int count = 0;
		while (!found){ // loops until an unoccupied thread is found
			int index = count%tp_size;
			if (threadpool[index].client == NULL && threadpool[index].has_work==0){ // if client and has_work are set to their defaults,
												// the thread can take work.
				found = 1;
				threadpool[index].client= c;
				threadpool[index].has_work = 1;
				pthread_cond_signal(&threadpool[index].cond);
				pthread_mutex_unlock(&threadpool[index].mutex);

			}
			count++;
		}
		// free(c) and free(threadpool) could be handled better?
		free(c);
	}
	free(threadpool);
}

