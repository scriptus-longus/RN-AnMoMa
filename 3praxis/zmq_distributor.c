#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#include "util.h"
#include "counter.h"

typedef struct {
  char *word;
  uint32_t n;
} pair_t;

void combine(dict_t *d, char *str) {
  size_t len = strlen(str);
  size_t head = 0;

  char word_buffer[255];
  char num_buffer[255];

  while (head < len) {
    memset(word_buffer, 0x0, 255);
    memset(num_buffer, 0x0, 255);

    int word_len = 0;
    int num_len = 0;

    while(islower(str[head + word_len]) && !isdigit(str[head + word_len])) {
      word_len++;
    }


    while(!islower(str[head + word_len + num_len]) && isdigit(str[head + word_len + num_len])) {
      num_len++;
    }

    memcpy(word_buffer, str + head, word_len);
    memcpy(num_buffer, str + head + word_len, num_len);

    int n = atoi(num_buffer);

    dict_add(d, word_buffer, n);
    head += word_len + num_len;
  }
}


void psorted_and_free(dict_t *counts) {
  printf("word,frequency\n");

  while (counts->used > 0) {
    char *word = NULL;
    int idx = 0;
    int current_max = 0;

    for (size_t i = 0; i < counts->used; i++) {
      char *current_word = counts->keys[i];
      int current_count = counts->values[i];

      if (current_count > current_max) {
        idx = i;
        current_max = current_count;
        word = current_word;
      } 

      if (current_count == current_max && strcmp(current_word, word) < 0) {
        idx = i;
        current_max = current_count;
        word = current_word;
      }
    }

    printf("%s,%d\n", word, current_max);
    dict_remove_at(counts, idx);
  }
}


void add_chunk(void *context, dict_t *d, char *chunk, int port) {
  size_t chunk_len = strlen(chunk);
  
  // configure uri
  char uri[50];
  memset(uri, 0x0, 50);
  sprintf(uri, "tcp://localhost:%d", port);

  // strings to send
  string_t *map_req = string_create("map");
  string_t *red_req = string_create("red");

  // connect
  void *requester = zmq_socket(context, ZMQ_REQ);

  zmq_connect(requester, uri);
 
  string_append(map_req, chunk); 
  s_send(requester, map_req->str);
  char *map_resp = s_recv(requester);


  string_append(red_req, map_resp);
  s_send(requester, red_req->str);
  char *red_resp = s_recv(requester);
  
  
  combine(d, red_resp);

  // cleanup
  free(map_resp);
  free(red_resp);

  string_free(map_req);
  string_free(red_req);

  zmq_close(requester);
}


void kill_all(void *context, int *ports, int n_ports) {
  char uri[50];

  for (int i = 0; i < n_ports; i++) {
    memset(uri, 0x0, 50);
    sprintf(uri, "tcp://localhost:%d", ports[i]);
    void *requester = zmq_socket(context, ZMQ_REQ);

    zmq_connect(requester, uri);

    s_send(requester, "rip\x00");
    char *resp = s_recv(requester);
    free(resp);
    // TODO: maybe check response

    zmq_close(requester);
  }

  return;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("No file specified\n");
    return -1;
  }

  if (argc < 3) {
    printf("No ports given\n");
    return -1;
  }

  int ports[argc - 2];
  int n_ports = argc - 2;

  for (int i = 0; i < argc - 2; i++) {
    ports[i] = atoi(argv[i + 2]);
  }

  char *chunk = "Hello this is a random test. The best test, that is random!";

  void *context = zmq_ctx_new();
  
  //int port = atoi(argv[3]);
  
  int MAX_CHUNK_SIZE = 1400;

  FILE *fp = fopen(argv[1], "r");
  
  // get file size
  fseek(fp, 0, SEEK_END);
  size_t len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  // read content
  char *content = (char *)malloc(len + 8);
  memset(content, 0x0, len + 8);

  char ch;
  int idx = 0;
  while (1) {
    ch = fgetc(fp);
    if (ch == EOF) {
      break;
    }

    if (isprint(ch) || isspace(ch)) {
      content[idx] = ch;
      idx++;
    }
  }

  
  dict_t* counts = dict_create();
  int n_chunks = (len + (MAX_CHUNK_SIZE + 1)) / MAX_CHUNK_SIZE;

  for (int i = 1; i < n_chunks; i++) {
    ssize_t idx = i * MAX_CHUNK_SIZE;
    while(content[idx] != ' ' && idx >= 0) {
      idx--; 
    }

    if (idx < 0) {
      printf("[Error] File has too long words\n");
      return -1;
    }

    content[idx] = '\x00';
  }

  
  idx = 0;
  int nth_chunk = 0;
  while (content[idx] != '\x00') {
    char *chunk = strdup(content + idx);
    size_t chunk_len = strlen(content + idx);

    int port = ports[nth_chunk%n_ports];

    add_chunk(context, counts, chunk, port);
    //add_chunk(counts, chunk);

    free(chunk);
    idx += chunk_len + 1;
    nth_chunk++;
  }

  kill_all(context, ports, n_ports);

  psorted_and_free(counts);
  free(content);

  dict_free(counts);
  fclose(fp);


  zmq_ctx_destroy(context);
  return 0;
}
