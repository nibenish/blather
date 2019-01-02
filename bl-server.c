#include "blather.h"
#include <signal.h>
#include <pthread.h>

int signalled = 1;
server_t mission_control;

void shutdown_handler (int signo) {
	printf("Server Shutting Down\n");
	signalled = 0;
	server_shutdown(&mission_control);
}

void ping_handler (int signo) {
	server_ping_clients(&mission_control);
	server_remove_disconnected(&mission_control, DISCONNECT_SECS);
	alarm(ALARM_INTERVAL);
}

int main (int argc, char *argv[]) {
	// Set up shutdown signal handling
	struct sigaction signals = {0};
	signals.sa_handler = shutdown_handler;
	sigemptyset(&signals.sa_mask);
	sigaction(SIGTERM, &signals, NULL);
	sigaction(SIGINT, &signals, NULL);

	// Set up ping signal handler and signal blocking
	//struct sigaction ping_signals = {0};
	//ping_signals.sa_handler = ping_handler;
	//sigemptyset(&ping_signals.sa_mask);
	//sigaction(SIGALRM, &ping_signals, NULL);
	
	// Set up signal blocking to protect select function
	//sigset_t newMask, oldMask;
	//sigaddset(&newMask, SIGALRM);
	//sigaddset(&newMask, SIGTERM);
	//sigaddset(&newMask, SIGINT);

	// Get server name
	if (argc < 2) {
		printf("Usage: servername\n");
		exit(1);
	}
	char *server_name = argv[1];

	// Set up server control structure and start server
	server_start(&mission_control, server_name, DEFAULT_PERMS);
	//alarm(ALARM_INTERVAL);		// Alarm to Ping

	// Main loop
	while(signalled) {
		
		//sigprocmask(SIG_BLOCK, &newMask, &oldMask);	// Block signals temporarily
		server_check_sources(&mission_control);			// Check all sources
		//sigprocmask(SIG_SETMASK, &oldMask, NULL);		// Unblock signals

		// Handle join request if needed
		if (server_join_ready(&mission_control)) {
			int ret = server_handle_join(&mission_control);
			if (ret == -1) {
				printf("Error adding new client");
			}
			mission_control.join_ready = 0;
		}
		
		// Loop through clients and process message if ready
		for (int i = 0; i < mission_control.n_clients; i++) {
			if (server_client_ready(&mission_control, i)) {
				server_handle_client(&mission_control, i);
			}
		}
		
		server_tick(&mission_control);
	}
}
