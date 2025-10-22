#include <stdio.h>       // printf, perror
#include <stdlib.h>      // malloc, free, exit
#include <string.h>      // strlen, strcpy, strtok
#include <unistd.h>      // read, write, close
#include <arpa/inet.h>   // sockaddr_in, htons, INADDR_ANY
#include <sys/socket.h>  // socket, bind, listen, accept
#include <pthread.h>     // pthread_create, pthread_detach
#include <signal.h>	 // handle_sigint

//stores important client data
struct client_info {
	int filedesc;
	struct sockaddr_in address;
};

//stores thread data
struct thread_manager {
	int id;
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int has_work;
	struct client_info *client;
};

//custom dynamic array
struct dynarr{
	size_t size;
	size_t capacity;
	char** data;
};

//initializer for te dynamic array, sets an initial capacity
struct dynarr* init_dynarr(size_t init_cap){
	struct dynarr *dyn = malloc(sizeof(struct dynarr));
	dyn->size = 0;
	dyn->capacity = init_cap;
	dyn->data = malloc(sizeof(char*)*init_cap);
	return dyn;
}

//handles adding to the dynamic array
int dynarr_add(struct dynarr* dyn, char* str){
	//if the dynarr is full it doubles the capacity and reallocs it
	if (dyn->size == dyn->capacity){
		dyn->capacity = dyn->capacity*2;
		char** tmp = realloc(dyn->data, (sizeof(char*)*dyn->capacity));
		if (!tmp) return 0;
		dyn->data = tmp;
	}
	//adds the item to the dynarr
	dyn->data[dyn->size++] = str;
	return 1;
	
}

int parse_get(struct dynarr* dyn, struct client_info* client){
	//reading the first line of the http req
	char* req = dyn->data[0];
	char* saveptr;
	char* method = strtok_r(req," ",&saveptr);
	char* path = strtok_r(NULL," ",&saveptr);
	char* version = strtok_r(NULL," ",&saveptr);
	size_t content_length;
	char* content_buffer = malloc(4096);

	// if the path has a parent directory in its path, return with 403 forbidden
	if (strstr(path,"..")!=NULL){
		//send 403 forbidden
		char* forbidden = "<html><body><h1>403 Forbidden</h1></body></html>";
		sprintf(content_buffer,"HTTP/1.1 403 Forbidden\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s",strlen(forbidden),forbidden);
		send(client->filedesc,content_buffer,strlen(content_buffer),0);
		free(content_buffer);
		return -1;
	}
	//if the path is default, replace it with index.html
	if (strcmp(path,"/") == 0) path = "/index.html";

	//concatenate the proveded path with the servers root path
	char full_path[50] = "/home/kowi/learning/httpserver";
	strcat(full_path,path);

	//open the file
	FILE *fptr = fopen(full_path,"rb");

	// if the file opening fails return with 404
	if (fptr == NULL){
		//send 404 not found
                char* notf = "<html><body><h1>404 Not Found</h1></body></html>";
                sprintf(content_buffer,"HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s",strlen(notf),notf);
                send(client->filedesc,content_buffer,strlen(content_buffer),0);
                free(content_buffer);
                return -1;
	}

	//calculating the length of the file, resetting the fptr
	fseek(fptr, 0, SEEK_END);
	long fsize = ftell(fptr);
	rewind(fptr);

	//allocate enough for the file and read it
	char* fbuffer = malloc(fsize);
	fread(fbuffer,1,fsize,fptr);
	
	//return with 200 OK with the file contents
	sprintf(content_buffer,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s",fsize,fbuffer);
	send(client->filedesc,content_buffer,strlen(content_buffer),0);
	
	//free buffers and close file, return that it was successful
	free(fbuffer);
	free(content_buffer);
	fclose(fptr);
	return 1;
}

void *thread_run(void* arg){
	//starting the thread and storing a pointer to the thread
	struct thread_manager *t = (struct thread_manager*)arg;

	//for checking all threads start
	printf("Hello from Thread %d!\n",t->id);

	//will loop until program ends
	while (1){
		pthread_mutex_lock(&t->mutex);

		//locks the thread until it has work
		while(!t->has_work){
			pthread_cond_wait(&t->cond,&t->mutex);
		}
		
		//local pointer for the threads client
		struct client_info *local = t->client;

		// makes a string for the ip, based on how long it is
		char ip[INET_ADDRSTRLEN]; 

		// converts to readable ip, needs protocol, pointer to the addr value, string to store it in and length
        	inet_ntop(AF_INET, &local->address.sin_addr,ip,INET_ADDRSTRLEN); 

        	printf("Successful connection from %s...\n",ip);
		printf("Connection from %s is being handled by Thread %d.\n",ip,t->id);
		
		//buffer to store the request, not univeral, just proof of concept for simple gets
		char buffer[4096];
		ssize_t bytes_read = read(local->filedesc, buffer,sizeof(buffer)-1);

		//if it fails to read from the client it sends an error and drops connection
		if (bytes_read<=0){
			printf("There was an error while Thread %d processed its request: Couldn't read bytes.\n",t->id);
			close(local->filedesc);
			t->has_work = 0;
			free(t->client);
			t->client = NULL;
			pthread_mutex_unlock(&t->mutex);
			return NULL;
		}

		//null terminates the buffer
		buffer[bytes_read] ='\0';
		
		//make a dynarr
		struct dynarr *lines = init_dynarr(4);

		//loop here separates the lines of the http request to be parsed by the parse_get method
		int ptr = 0;
		int b_len = strlen(buffer);
		for(int i = 0; i < b_len; i++){
			if(buffer[ptr] == '\r'){
				break; // if the start of the next line is empty itll break because its done
			}
			if(buffer[i] == '\r'){
				buffer[i] = '\0';
				dynarr_add(lines, &buffer[ptr]);
				i++;
				ptr = i+1;
			}
		}
		//parses the data, eventually will need to change to check the method here instead of
		//in the parse_get method so it can handle other types of requests
		int res = parse_get(lines,t->client);

		//freeing the lines after its done
		free(lines->data);
		free(lines);

		//prints that the thread is finished, closes the file, frees everything then unlocks this thread for use
		printf("Thread %d has completed its work.\n",t->id);
        	close(local->filedesc);
		t->has_work = 0;
		free(t->client);
		t->client = NULL;
		pthread_mutex_unlock(&t->mutex);
        	
	}
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

	// stores the address family
	server_addr.sin_family = AF_INET;
	
	// stores the ip. sin_addr is a struct member in the addr struct, it is the name of an in_addr struct, which holds the ip at s_addr.
	server_addr.sin_addr.s_addr = INADDR_ANY;

	//orders the ip correctly
	server_addr.sin_port = htons(port);
	
	//so the port can be reused quickly
	int opt = 1;
	if (setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0){
		perror("setsockopt failed.");
		return 1;
	}

	// binding the socket
	// bind takes in the socket made, a pointer to the socket structure we made, but being cast as a more generic type of sockaddr
	// if it fails close server
	if (bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
		close(server);
		perror("socket couldnt bind.\n");
		return 1;
		
	}
	
	// freeing the socket when the server is closed
	//signal(SIGINT,handle_sigint); 

	//set up threads
	struct thread_manager threadpool[tp_size];
	for(int i = 0;i<tp_size;i++){
		threadpool[i].id=i;
		threadpool[i].has_work=0;
		pthread_mutex_init(&threadpool[i].mutex,NULL);
		pthread_cond_init(&threadpool[i].cond, NULL);
		pthread_create(&threadpool[i].thread, NULL, thread_run, &threadpool[i]);

	}

	// listening
	// tries to listen, if theres an error it returns -1
	if (listen(server, 5)<0){ 
		close(server);
		perror("couldnt listen.\n");
		return 1;
	}
	printf("server is listening on port %d.\n",port);

	// loop for accepting clients
	while(1){
		//accept a connection
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client = accept(server, (struct sockaddr*)&client_addr, &client_len);

		//if it fails to accept the client, just report it and move on
		if (client<0){ 
			perror("couldnt accept client.\n");
		}
		
		//allocating memory for the client
		struct client_info *c = malloc(sizeof(struct client_info));
		c->filedesc = client;
		c->address = client_addr;

		//managing resource assignment
		int found =0;
		int count = 0;

		//loops until an unoccupied thread is found
		while (!found){
			// will loop 0--n where n is # of threads
			int index = count%tp_size;
			//if client and has_work are set to defaults, the thread can take work
			if (threadpool[index].client == NULL && threadpool[index].has_work==0){
				found = 1;
				threadpool[index].client= c;
				threadpool[index].has_work = 1;
				pthread_cond_signal(&threadpool[index].cond);
			}
			count++;
		}
	}
	close(server);
}
