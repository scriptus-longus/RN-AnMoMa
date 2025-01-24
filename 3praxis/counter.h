#ifndef COUNTER_INCLUDE_H
#define COUTNER_INCLUDE_H

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#include "util.h"


dict_t* count_words(char *str) {
  size_t len = strlen(str);
  size_t start = 0;
  size_t end = start;

  dict_t* ret = dict_create();
  char word[250];

  while (end < len) {
    if (isalpha(str[end]) && str[end] != '!' && str[end] != '?') {
      end++;
      continue;
    }

    if (start == end) {
      start = end + 1;
      end = start;
      continue;
    }

    memset(word, 0x0, 250);
    memcpy(word, &(str[start]), end - start);

    // make lowercase
    for (int i = 0; i < end-start; i++) {
      word[i] = tolower(word[i]);
    }

    dict_add(ret, word, 1);

    start = end + 1;
    end = start;
  }


  if (end > start) {
    memset(word, 0x0, 250);
    memcpy(word, &(str[start]), end - start);

    for (int i = 0; i < end-start; i++) {
      word[i] = tolower(word[i]);
    }

    dict_add(ret, word, 1);
  }

  return ret;
}

string_t* map_words(char *str, size_t start, size_t len) {
  char *substr = strdup(str + start);

  dict_t* counts = count_words(substr);

  string_t *ret = string_create("");

  for (size_t i = 0; i < counts->used; i++) {

    string_append(ret, counts->keys[i]);

    int n = counts->values[i];
    while(n > 0) {
      string_append(ret, "1");
      n--;
    }
  }

  dict_free(counts);
  free(substr);
  return ret;
}

string_t* reduce(char *str) {
  size_t len = strlen(str);
  size_t head = 0;

  dict_t *counts = dict_create();

  char buffer[255];
  string_t* ret = string_create(""); 

  // reduce
  while (head < len) {
    memset(buffer, 0x0, 255);
    int word_len = 0;
    int n = 0;

    while(islower(str[head + word_len]) && !isdigit(str[head + word_len])) {
      word_len++;
    }


    while(!islower(str[head + word_len + n]) && isdigit(str[head + word_len + n])) {
      n++;
    }

    memcpy(buffer, str + head, word_len);

    dict_add(counts, buffer, n);
    head += word_len + n;
  }

  // make string
  for (size_t i = 0; i < counts->used; i++) {
    memset(buffer, 0x0, 255);
    sprintf(buffer, "%d", counts->values[i]);

    string_append(ret, counts->keys[i]);
    string_append(ret, buffer);
  }

  dict_free(counts);
  return ret;

}
#endif
