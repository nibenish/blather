#include "blather.h"
#include <semaphore.h>
#include <pthread.h>

/*
// server_t: data pertaining to server operations
typedef struct {
  char server_name[MAXPATH];    // name of server which dictates file names for joining and logging
  int join_fd;                  // file descriptor of join file/FIFO
  int join_ready;               // flag indicating if a join is available
  int n_clients;                // number of clients communicating with server
  client_t client[MAXCLIENTS];  // array of clients populated up to n_clients
  int time_sec;                 // ADVANCED: time in seconds since server started
  int log_fd;                   // ADVANCED: file descriptor for log
  sem_t *log_sem;               // ADVANCED: posix semaphore to control who_t section of log file
} server_t;
 */


client_t *server_get_client(server_t *server, int idx) {
	// Check index is in bounds and return desired client
	check_fail(idx > server->n_clients-1, 1, "Index out of bounds");
	return &server->client[idx];
}


void server_start(server_t *server, char *server_name, int perms) {
	printf("Starting Server\n");

	// Check server_name length and copy into server struct
	check_fail((strlen(server_name) + 1) > MAXPATH, 1, "Server name '%s' exceeds %d characters", server_name, MAXPATH);
	strcpy(server->server_name, server_name);

	// Open and create fifo
	char fifo_name[MAXPATH] = "";
	strcpy(fifo_name, server_name);
	strcat(fifo_name, ".fifo");
	remove(fifo_name);         // remove if already present
	mkfifo(fifo_name, perms);
	int fifo_fd = open(fifo_name, O_RDWR);
	check_fail(fifo_fd == -1, 1, "Error opening %s", fifo_name);
	server->join_fd = fifo_fd;
	server->join_ready = 0;


	// Create log file
	char log_name [MAXPATH + 5] = "";
	strcpy(log_name, server_name);
	strcat(log_name, ".log");
	int log_fd = open(log_name, O_CREAT | O_WRONLY, DEFAULT_PERMS);
	check_fail(log_fd == -1, 1, "Error opening %s", log_name);
	lseek(log_fd, 0, SEEK_END);		// Ensures file is set to append
	server->log_fd = log_fd;

	// Create semaphore
	sem_t *sem;
	sem = sem_open(server_name, O_CREAT, DEFAULT_PERMS, 1);
	server->log_sem = sem;
	
	// Write initial who_t structure to file
	server_write_who(server);
}


void server_shutdown(server_t *server) {
	printf("Server Shutting Down\n");
	// Close join_fd
	int ret = close(server->join_fd);
	check_fail(ret == -1, 1, "Error closing %s", server->server_name);
	server->join_fd = -1;

	// Broadcast shutdown message
	mesg_t mesg = {0};
	memset(&mesg, 0, sizeof(mesg_t));
	mesg.kind = BL_SHUTDOWN;
	server_broadcast(server, &mesg);
	
	// Write exiting who_t structure
	server_write_who(server);

	// Close log file and log semaphore
	close(server->log_fd);
	sem_close(server->log_sem);
	sem_unlink(server->server_name);
	server->log_sem = NULL;
}


int server_add_client(server_t *server, join_t *join) {
	printf("Server Adding Client: %s\n", join->name);
	// Check capacity
	if (server->n_clients == MAXCLIENTS) {
		perror("Reached maximum capacity of clients on server");
		return -1;
	}

	// Open fifos
	int to_server = open(join->to_server_fname, O_CREAT | O_RDONLY);
	check_fail(to_server == -1, 1, "Error opening %s", join->to_server_fname);
	int to_client = open(join->to_client_fname, O_CREAT | O_WRONLY);
	check_fail(to_client == -1, 1, "Error opening %s", join->to_client_fname);

	// Create new client
	client_t new = {0};
	memset(&new, 0, sizeof(client_t));
	strcpy(new.name, join->name);
	strcpy(new.to_client_fname, join->to_client_fname);
	strcpy(new.to_server_fname, join->to_server_fname);
	new.to_server_fd = to_server;
	new.to_client_fd = to_client;
	new.data_ready = 0;



	// Put new client into client[] and return
	server->client[server->n_clients] = new;
	server->n_clients++;

	// broadcast join message
	mesg_t mesg = {0};
	memset(&mesg, 0, sizeof(mesg));
	mesg.kind = BL_JOINED;
	strcpy(mesg.name, new.name);
	server_broadcast(server, &mesg);

	return 0;
}


int server_remove_client(server_t *server, int idx) {

	// Get client
	client_t *client = server_get_client(server, idx);
  printf("Removing Client: %s\n", client->name);
	// Close fifos and set to null
	int ret = close(client->to_server_fd);
	check_fail(ret == -1, 1, "Error closing %s", client->to_server_fname);
	client->to_server_fd = -1;
	ret = close(client->to_client_fd);
	check_fail(ret == -1, 1, "Error closing %s", client->to_client_fname);
	client->to_client_fd = -1;

	// Shift clients and decrement clients and return
	for (int i = idx+1; i < server->n_clients; i++) {
		server->client[i-1] = server->client[i];
	}
	server->n_clients --;
	return 0;
}


int server_broadcast(server_t *server, mesg_t *mesg) {
	// Broadcast message
	for (int i = 0; i < server->n_clients; i++) {
		client_t *client = server_get_client(server, i);
		int ret = write(client->to_client_fd, mesg, sizeof(mesg_t));
		if (ret == -1) {
			printf("Error sending message to %s", client->name);
			return -1;
		}
	}
	return 0;
}


void server_check_sources(server_t *server) {

	// Create fd_set and find max_fd
	fd_set set;
	memset(&set, 0, sizeof(fd_set));
	FD_ZERO(&set);
  int maxfd = 0;
	for (int i = 0; i < server->n_clients; i++) {
		client_t *client = server_get_client(server, i);
		FD_SET(client->to_server_fd, &set);
		if (client->to_server_fd > maxfd) {
			maxfd = client->to_server_fd;
		}
	}
	FD_SET(server->join_fd, &set);
	if(server->join_fd > maxfd) {
		maxfd = server->join_fd;
	}


	// Select all incoming fifos, wait until something happens
	int ret = select(maxfd+1, &set, NULL, NULL, NULL);
	check_fail(ret == -1, 1, "Error with select()");
	// Check if someone wants to join
	if (FD_ISSET(server->join_fd, &set)) {
		server->join_ready = 1;
	}
	// Iterate through all clients to see if they have something to say
	for (int i =0; i < server->n_clients; i++) {
		client_t *client = server_get_client(server, i);
		if (FD_ISSET(client->to_server_fd, &set)) {
			client->data_ready = 1;
		}
	}
}


int server_join_ready(server_t *server) {
	return server->join_ready;
}


int server_handle_join(server_t *server) {
	join_t new = {0};
	memset(&new, 0, sizeof(join_t));
	int ret = read(server->join_fd, &new, sizeof(join_t));
	check_fail(ret == -1, 1, "Error reading join request");
	ret = server_add_client(server, &new);
	return ret;
}


int server_client_ready(server_t *server, int idx) {
	client_t *client = server_get_client(server, idx);
	return client->data_ready;
}


int server_handle_client(server_t *server, int idx) {
	// Get client and get message from client to_server fifo
	client_t *client = server_get_client(server, idx);
	mesg_t mesg = {0};
	memset(&mesg, 0, sizeof(mesg_t));
	int ret = read(client->to_server_fd, &mesg, sizeof(mesg_t));
	check_fail(ret == -1, 1, "Error reading from %s\n", client->to_server_fname);

	// Route based on message type
	mesg_kind_t type = mesg.kind;
	if (type == 10 || type == 20 || type == 30 || type == 40 || type == 50) {	// Whitelist of legal broadcasts
		server_broadcast(server, &mesg);
		server_log_message(server, &mesg);
		if (type == 30) {
			server_remove_client(server, idx);
		}
	}

	// Update data_ready and last_contact_time
	client->data_ready = 0;
	client->last_contact_time = server->time_sec;

	return 0;
}


void server_tick(server_t *server) {
	// Increment server time
	server->time_sec ++;
}


void server_ping_clients(server_t *server) {
	// Construct mesg_t of type BL_PING
	mesg_t mesg = {0};
	memset(&mesg, 0, sizeof(mesg_t));
	mesg_kind_t kind = BL_PING;
	mesg.kind = kind;
	server_broadcast(server, &mesg);
}


void server_remove_disconnected(server_t *server, int disconnect_secs) {
	for (int i = 0; i < server->n_clients; ) {
		client_t *client = server_get_client(server, i);
		if (client->last_contact_time-server->time_sec > disconnect_secs) {
			// Remove user
			server_remove_client(server, i);

			// Send broadcast message
			mesg_t mesg = {0};
			memset(&mesg, 0, sizeof(mesg_t));
			mesg_kind_t kind = BL_DISCONNECTED;
			mesg.kind = kind;
			strcpy(mesg.name, client->name);
			server_broadcast(server, &mesg);
			// Do not increment, all clients will be shifted by server_remove_client
		}
		else {
			i++;		// Only increment if client is still active
		}
	}
}


void server_write_who(server_t *server) {
	// Acquire semaphore
	sem_t *sem = server->log_sem;
	int ret = sem_wait(sem);
	check_fail(ret == -1, 1, "Error acquiring semaphore\n");

	// Create who_t structure to record data
	who_t who;
	memset(&who, 0, sizeof(who_t));
	for (int i = 0; i < server->n_clients; i++) {
		client_t *client = server_get_client(server, i);
		strcpy(who.names[i], client->name);
	}
	who.n_clients = server->n_clients;

	// Writes data to log
	ret = pwrite(server->log_fd, &who, sizeof(who_t), 0);
	check_fail(ret == -1, 1, "Error writing who_t to log\n");

	// Release semaphore
	ret = sem_post(sem);
	check_fail(ret == -1, 1, "Error releasing semaphore\n");
}


void server_log_message(server_t *server, mesg_t *mesg) {
	// Acquire semaphore
	sem_t *sem = server->log_sem;
	int ret = sem_wait(sem);
	check_fail(ret == -1, 1, "Error acquiring semaphore\n");

	// Log binary data
	ret = write(server->log_fd, mesg, sizeof(mesg_t));
	check_fail(ret == -1, 1, "Error writing message to log");

	// Release semaphore
	ret = sem_post(sem);
	check_fail(ret == -1, 1, "Error releasing semaphore\n");
}
