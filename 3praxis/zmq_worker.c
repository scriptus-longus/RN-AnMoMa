#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#include "util.h"
#include "counter.h"

struct worker_args {
  void* context;
  uint16_t port;
}; 

void unpack(char *msg, package_t *pack) {
  size_t msg_len = strlen(msg);
  pack->type = UNDEF;

  if (strlen(msg) < 3)  {
    pack->payload = NULL;
    return;
  }
   
  if (msg[0] == 'm' && msg[1] == 'a' && msg[2] == 'p') {
    pack->type = MAP;
    pack->payload = strdup(msg + 3);
    return;
  }

  if (msg[0] == 'r' && msg[1] == 'e' && msg[2] == 'd') {
    pack->type = RED;
    pack->payload = strdup(msg + 3);
    return;
  }

  if (msg[0] == 'r' && msg[1] == 'i' && msg[2] == 'p') {
    pack->type = RIP;
    pack->payload = NULL;
  }
  
  return;
}


void* worker_routine(void *arguments) {
  struct worker_args *args = (struct worker_args*)arguments;
  void *context = args->context;
  uint16_t port = args->port;

  free(arguments);
  args = NULL;

  char uri[15];
  char *response = (char*)malloc(MAX_BUF_SIZE);
  sprintf(uri, "tcp://*:%d", port);

  void *receiver = zmq_socket(context, ZMQ_REP);
  int rc = zmq_bind(receiver, uri);

  if (rc != 0) {
    printf("[Error] could not bind to port %d\n", port);
    zmq_close(receiver);
    return NULL;
  }

  package_t *package = (package_t*)(malloc(sizeof(package_t)));

  printf("Starting worker on port %s\n", uri);
  while (1) {
    char *data = s_recv(receiver);
    //printf("Received: %s\n\n", data);

    //  unpack data
    memset(package, 0x0, sizeof(package_t));
    unpack(data, package);
    free(data);

    // process data
    memset(response, 0x0, MAX_BUF_SIZE);
    if (package->type == MAP) {
      printf("Got MAP request");
      size_t payload_len = strlen(package->payload);

      string_t *mapped_str = map_words(package->payload, 0, payload_len);

      sprintf(response, mapped_str->str);

      string_free(mapped_str);
      free(package->payload);
    } 

    if (package->type == RED) {
      printf("Got RED request\n");
      string_t *red_str = reduce(package->payload);

      sprintf(response, red_str->str);

      string_free(red_str);
      free(package->payload);
    }

    if (package->type == RIP) {
      printf("Got RIP request. Shutting down\n");
      sprintf(response, "rip");
    }


    //printf("Sending %s\n\n", response);
    s_send(receiver, response);

    if (package->type == RIP) {
      break;
    }
  }

  printf("Exiting worker...\n");

  free(package);
  free(response);
  zmq_close(receiver);

  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("No Ports specified\nUse: ./zmq_worker <port 1> <port 2> ... <port n>\n");
  }

  void *context = zmq_ctx_new();
  pthread_t tid[argc-1];


  for (int i = 1; i < argc; i++) {
    int port = atoi(argv[i]);

    if (0 < port && port < 0xffff) {
      struct worker_args *args = malloc(sizeof(struct worker_args));
      args->context = context;
      args->port = (uint16_t)port;
      printf("Creating Thread for socket on Port %d\n", port);
      
      pthread_create(&tid[i-1], NULL, worker_routine, (void *)args);
    }
  }

  for(int i = 1; i < argc; i++) {
    pthread_join(tid[i-1], NULL);
  }


  zmq_ctx_destroy(context);
  return 0;
}
