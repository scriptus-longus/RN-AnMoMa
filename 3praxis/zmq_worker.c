#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

struct worker_args {
  void* context;
  uint16_t port;
}; 

void* worker_routine(void *arguments) {
  struct worker_args *args = (struct worker_args*)arguments;
  void *context = args->context;
  uint16_t port = args->port;

  char uri[15];
  sprintf(uri, "tcp://*:%d", port);

  void *receiver = zmq_socket(context, ZMQ_REP);
  int rc = zmq_bind(receiver, uri);

  if (rc != 0) {
    printf("[Error] could not bind to port %d\n", port);
    zmq_close(receiver);
    return NULL;
  }

  while (1) {
    char buffer[1500];
    zmq_recv(receiver, buffer, 1500, 0);
    printf("Received some stuff\n");
    zmq_send(receiver, " ", 1, 0);
  }

  zmq_close(receiver);
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("No ports provied!");
    return -1;
  }

  void *context = zmq_ctx_new();

  pthread_t tid[argc-1];

  for (int i = 1; i < argc; i++) {
    int port = atoi(argv[i]);

    if (0 < port && port < 0xffff) {
      struct worker_args *args = malloc(sizeof(struct worker_args));
      args->context = context;
      args->port = (uint16_t)port;

      pthread_create(&tid[i-1], NULL, worker_routine, (void *)args);
    }
  }

  for(int i = 1; i < argc; i++) {
    pthread_join(tid[i-1], NULL);
  }

  zmq_ctx_destroy(context);

  return 0;
}
