#ifndef UTIL_H_INCLUDE
#define UTIL_H_INCLUDE

#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_BUF_SIZE 1500

enum msg_type {
  MAP,
  RED, 
  RIP,
  UNDEF
} typedef msg_type;

struct package {
  msg_type type;
  char *payload;
} typedef package_t;


struct dynamic_array {
  void *array;
  size_t type_size;
  size_t size;
  size_t used;
} typedef vector_t;


struct dynamic_string_type {
  char *str;
  size_t size;
} typedef string_t;

struct dictioray {
  char **keys;
  int *values;

  size_t size;
  size_t used;
} typedef dict_t;

dict_t* dict_create() {
  dict_t *ret = (dict_t *)malloc(sizeof(dict_t));

  ret->keys = (char **)malloc(5 * (sizeof(char *)));
  ret->values = (int *)malloc(5 * sizeof(int));

  ret->size = 5;
  ret->used = 0;

}

void dict_free(dict_t *d) {
  for (int i = 0; i < d->used; i++) {
    free(d->keys[i]);
    d->keys[i] = NULL;
  }

  free(d->values);
  free(d->keys);

  d->keys = NULL;
  d->values = NULL;

  free(d);
}

int dict_find_value(dict_t *d, char *word) {
  for (int i = 0; i < d->used; i++) {
    if (strcmp(d->keys[i], word) == 0) {
      return d->values[i];
    }
  }

  return -1;
}

int dict_find_idx(dict_t *d, char *word) {
  for (int i = 0; i < d->used; i++) {
    if (strcmp(d->keys[i], word) == 0) {
      return i;
    }
  }

  return -1;

}

int dict_add(dict_t *d, char *word, int i) {
  /*for (size_t i = 0; i < d->used; i++) {
    if (strcmp(d->keys[i], word) == 0) {
      d->values[i] += i;
      return 1;
    }
  }*/
  int idx = dict_find_idx(d, word);
  if (idx != -1) {
    d->values[idx] += i; 
    return 0;
  }

  if (d->used >= d->size) {
    d->keys = (char **)realloc(d->keys, sizeof(char *)*d->size*2);
    d->values = (int *)realloc(d->values, sizeof(int)*d->size*2);
    d->size *= 2;
  }


  d->keys[d->used] = strdup(word);
  d->values[d->used] = i;
  
  d->used++;
  return 1;
}

int dict_remove_at(dict_t *d, int idx) {
  if (idx >= 0 && idx < d->used) {
    free(d->keys[idx]);

    d->keys[idx] = NULL;
    d->values[idx] = 0;

    if (idx < d->used-1) {
      memmove(&(d->keys[idx]), &(d->keys[idx+1]), (d->used - idx - 1) * sizeof(char *));
      memmove(&(d->values[idx]), &(d->values[idx+1]), (d->used - idx - 1) * sizeof(int));
    }

    d->used--;
  }
   
  return 1;
}

int dict_remove(dict_t *d, char *word) {
  int idx = dict_find_idx(d, word);

  if (idx == -1) {
    return 0;
  }

  return dict_remove_at(d, idx);
}



/*
void dynamic_init(vector_t *v, size_t size) {
  v->array = calloc(5, size);
  v->type_size = size;
  v->size = 5;
  v->used = 0;
  return;
}

vector_t* dynamic_create(size_t size) {
  vector_t *ret = (vector_t *)malloc(sizeof(vector_t));

  dynamic_init(ret, size);

  return ret;
}

void dynamic_free(vector_t* v) {
  free(v->array);
  free(v);
  return;
}

int dynamic_add(vector_t* v, void *data) {
  assert(v->type_size > 0);
  if (v->array == NULL || v->size == 0) {
    return -1;
  }

  // check if space is free
  if (v->used >= v->size) {
    v->array = realloc(v->array, v->type_size * v->size * 2);    

    v->size *= 2;
  }


  // find free
  memmove(v->array + v->used * v->type_size, data, v->type_size);

  v->used++;
  return 2;
}

int dynamic_remove(vector_t* v, size_t idx) { 
  // TODO: fix remove

  void *element = v->array + idx * v->type_size;
  void *next = v->array + (idx+1) * v->type_size;
  size_t remaining_size = v->size * v->type_size - (idx+1) * v->type_size;

  if (remaining_size > 0) {
    memmove(element, next, remaining_size);
  }

  v->used--;
  return 1;
}

void * dynamic_get(vector_t* v, size_t idx) {
  if (v->size <= idx) {
    return NULL;
  }

  return v->array + idx * v->type_size;
}

int dynamic_find(vector_t* v, void *data) {
  for (int i = 0; i < v->size; i++) {
    if (memcmp(v->array + i * v->type_size, data, v->type_size)) {
      return i;
    }
  }

  return -1;
}*/

string_t* string_create(char *str) {
  size_t len = strlen(str);

  string_t *ret = (string_t *)calloc(1, sizeof(string_t));
  ret->str = strdup(str);
  ret->size = len;
}

int string_append(string_t *str, char *data) {
  size_t data_len = strlen(data);
  size_t source_len = strlen(str->str);

  size_t new_len = source_len + data_len;

  str->str = (char *)realloc(str->str, new_len + 2);


  memcpy(str->str + source_len, data, data_len);
  str->str[new_len] = '\x00';
  str->size = new_len;
  return 1;
}

/*vector_t* string_split(string_t *str, char delim) {
  vector_t *splits = dynamic_create(sizeof(string_t));

  return splits;
}*/

void string_free(string_t *str) {
  free(str->str);

  str->str = NULL;
  str->size = 0;

  free(str);
}

char *s_recv (void *socket) {
  char buffer[MAX_BUF_SIZE];
  memset(buffer, 0x0, MAX_BUF_SIZE);

  int msg_len = zmq_recv(socket, buffer, MAX_BUF_SIZE, 0);

  if (msg_len == -1)
    return NULL;

  if (msg_len > MAX_BUF_SIZE-1)
    msg_len = MAX_BUF_SIZE-1;

  buffer[msg_len] = '\x00';
 
  return strdup(buffer);
}

int s_send (void *socket, char *string) {
  int len = strlen(string);
  int sent_bytes = zmq_send(socket, string, len + 1, 0);

  if (sent_bytes != len) 
    return -1;

  return 0;
}

#endif
