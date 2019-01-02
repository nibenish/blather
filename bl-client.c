//
// bl-client.c
//
// Nick Benish
//

// INCLUDES //
#include "blather.h"
#include <pthread.h>

// SET UP //
simpio_t simpio_actual = {0};
simpio_t *simpio = &simpio_actual;

pthread_t user_thread = {0};
pthread_t server_thread = {0};

void *user_thread_fn(void *param);
void *server_thread_fn(void *param);

static char *clientname = "";
static int to_client_fd = -1;
static int to_server_fd = -1;

// MAIN //
int main(int argc, char *argv[]) {
	
  // Check command line arguments	
  if (argc < 3) {
    printf("%d", argc);
    fprintf(stderr,"usage: hostname client %d\n", argc);
    exit(1);
  }

  // Read name of server and name of user from command line args
  clientname = argv[2];
  char *hostname = argv[1];

  // Create and open server join fifo
  char join_fifo[MAXPATH] = "";
  strcpy(join_fifo, hostname);
  strcat(join_fifo, ".fifo");
  int join_fifo_fd = open(join_fifo, O_WRONLY);
  check_fail(join_fifo_fd == -1, 1, "Error opening %s", join_fifo);

  // Prepare to-server and to-client fifo names
  char to_client_fifo[MAXPATH] = "";
  char pidbuf[MAXPATH] = "";
  sprintf(pidbuf, "%d", getpid());
  strcpy(to_client_fifo, pidbuf);
  strcat(to_client_fifo, ".client.fifo");
  char to_server_fifo[MAXPATH] = "";
  strcpy(to_server_fifo, pidbuf);
  strcat(to_server_fifo, ".server.fifo");
  
  // Create and open fifos
  mkfifo(to_client_fifo, DEFAULT_PERMS);  
  mkfifo(to_server_fifo, DEFAULT_PERMS);  
  to_client_fd = open(to_client_fifo, O_RDWR);
  to_server_fd = open(to_server_fifo, O_RDWR);

  // Write a join_t request to the server fifo
  join_t join_request = {0};
  strcpy(join_request.name, clientname);
  strcpy(join_request.to_client_fname, to_client_fifo);
  strcpy(join_request.to_server_fname, to_server_fifo);
  write(join_fifo_fd, &join_request, sizeof(join_t));
  close(join_fifo_fd);

  // Simpio configuration
  char prompt[MAXNAME] = "";
  snprintf(prompt, MAXNAME, "%s>> ", clientname);  // Create prompt string
  simpio_set_prompt(simpio, prompt);               // Set the prompt
  simpio_reset(simpio);                            // Initialize io
  simpio_noncanonical_terminal_mode();             // Set the terminal into a compatible mode

  // Start a user thread to read input
  pthread_create(&user_thread, NULL, user_thread_fn, NULL);

  // Start a server thread to listen to the server
  pthread_create(&server_thread, NULL, server_thread_fn, NULL);

  // Wait for threads to return
  pthread_join(user_thread, NULL);
  pthread_join(server_thread, NULL);
  
  // Restore standard terminal output
  simpio_reset_terminal_mode();
  //printf("\n");

  close(to_server_fd);
  close(to_client_fd);

} /* main() */


// USER THREAD //

void *user_thread_fn(void *param) {
  
  // Read user input 
  while(!simpio->end_of_input){
    simpio_reset(simpio);
    iprintf(simpio, ""); // print prompt
    while(!simpio->line_ready && !simpio->end_of_input){ // read until line is complete
      simpio_get_char(simpio);
    }
    if(simpio->line_ready) { // create a mesg_t with the line and write it to the to-server FIFO
      //iprintf(simpio, "[%s] : %s\n", clientname, simpio->buf);
      mesg_t mesg = {0};
      mesg_kind_t kind = BL_MESG;
      mesg.kind = kind;
      strcpy(mesg.name, clientname);
      strcpy(mesg.body, simpio->buf);

      int ret = write(to_server_fd, &mesg, sizeof(mesg_t));
      check_fail(ret == -1, 1, "Error writing to server");
    }
  } /* while(...) */

  // Write a BL_DEPARTED mesg_t into to_server
  mesg_t mesg = {
    .kind = BL_DEPARTED,
    .body = "",
  };
  strcpy(mesg.name, clientname);
  write(to_server_fd, &mesg, sizeof(mesg_t));

  // cancel the server thread
  pthread_cancel(server_thread);
  
} /* user_thread_fn() */



void *server_thread_fn(void *param) {
  //
  bool server_condvar = true;
  while(server_condvar) { 
    // Read a mesg_t from to_client fifo
    mesg_t read_mesg = {0};
    int ret = read(to_client_fd, &read_mesg, sizeof(mesg_t));
    check_fail(ret == -1, 1, "Error reading message");
    mesg_kind_t type = read_mesg.kind;

    // Print appropriate response to terminal with simpio
    switch(type) {
      case BL_MESG :		
	    iprintf(simpio, "[%s] : %s\n", read_mesg.name, read_mesg.body);
        break;
      case BL_JOINED :
        iprintf(simpio, "-- %s JOINED --\n", read_mesg.name);
        break;
      case BL_DEPARTED :	
        if (strcmp(read_mesg.name, clientname) != 0) {
          iprintf(simpio, "-- %s DEPARTED --\n", read_mesg.name);
        }
	      break;
      case BL_SHUTDOWN :	
        iprintf(simpio, "!!! server is shutting down !!!\n");
        server_condvar = false;  // until a SHUTDOWN mesg_t is read
        break;
      case BL_DISCONNECTED :	
        iprintf(simpio, "-- %s DISCONNECTED --\n", read_mesg.name);
        break;
      case BL_PING : ;	
        mesg_t mesg = {0};
        mesg_kind_t kind = BL_PING;
        mesg.kind = kind;
        int ret = write(to_server_fd, &mesg, sizeof(mesg_t));
        check_fail(ret == -1, 1, "Error writing to file");
        break;
      default :
		iprintf(simpio, "ERROR: server_thread in bl-client received invalid mesg_t");
		break;
    }
  } /* while(...) */

  // cancel the user thread
  pthread_cancel(user_thread);
  
} /* server_thread */

/* bl-client.c */
