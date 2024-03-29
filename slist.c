// based on cs3650 starter code

#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#include "slist.h"

slist* s_cons(const char* text, slist* rest) {
  slist* xs = malloc(sizeof(slist));
  xs->data = strdup(text);
  xs->next = rest;
  return xs;
}

void s_free(slist* xs) {
  if (xs == 0) {
    return;
  }

  s_free(xs->next);
  free(xs->data);
  free(xs);
}

slist* s_split(const char* text, char delim) {
  if (*text == 0) {
    return 0;
  }

  int plen = 0;
  while (text[plen] != 0 && text[plen] != delim) {
    plen += 1;
  }

  int skip = 0;
  if (text[plen] == delim) {
    skip = 1;
  }

  slist* rest = s_split(text + plen + skip, delim);
  char* part = alloca(plen + 2);
  memcpy(part, text, plen);
  part[plen] = 0;

  return s_cons(part, rest);
}