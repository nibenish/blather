#include "blather.h"

int main (int argc, char *argv[]) {
  // Get server name
	if (argc < 2) {
		printf("Usage: servername\n");
		return 1;
	}
	char *server_name = argv[1];

  char log_name [MAXPATH + 5];
	strcpy(log_name, server_name);
	strcat(log_name, ".log");
  int log_fd = open(log_name, O_RDONLY, DEFAULT_PERMS);
	check_fail(log_fd == -1, 1, "Error opening %s", log_name);

  who_t who = {0};
  read(log_fd, &who, sizeof(who_t));
  printf("%d CLIENTS\n", who.n_clients);
  for (int i = 0; i < who.n_clients; i++) {
    printf("%d: %s\n", i, who.names[i]);
  }

  mesg_t mesg = {0};
  int ret = 0;
  while ((ret = read(log_fd, &mesg, sizeof(mesg_t))) > 0) {
    mesg_kind_t type = mesg.kind;

    switch(type) {
      case 10 :
	      printf("[%s] : %s\n", mesg.name, mesg.body);
        break;
      case 20 :
        printf("-- %s JOINED --\n", mesg.name);
        break;
      case 30 :
        printf("-- %s DEPARTED --\n", mesg.name);
	      break;
      case 40 :
        printf("!!! server is shutting down !!!\n");
        break;
      case 50 :
        printf("-- %s DISCONNECTED --\n", mesg.name);
        break;
      default:
        break;
     }
  }
  return 0;
}
