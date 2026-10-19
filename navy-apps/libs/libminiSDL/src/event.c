#include <NDL.h>
#include <SDL.h>
#include <string.h>
#include <stdio.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

#define KEYNUM (sizeof(keyname) / sizeof(keyname[0]))
static uint8_t key_status[KEYNUM] = {0};

int SDL_PushEvent(SDL_Event *ev) {
  (void)ev;
  return 0;
}

int SDL_PollEvent(SDL_Event *ev) {
  if (ev == NULL) return 0;

  char buf[64] = {0};
  if (NDL_PollEvent(buf, sizeof(buf) - 1) <= 0 || buf[0] != 'k') {
    return 0;
  }

  char key_type;
  char key_buf[32];
  if (sscanf(buf, "k%c %31s", &key_type, key_buf) != 2) {
    return 0;
  }

  switch (key_type) {
    case 'u': ev->type = SDL_KEYUP; break;
    case 'd': ev->type = SDL_KEYDOWN; break;
    default: return 0;
  }

  for (int i = 1; i < (int)KEYNUM; i++) {
    if (strcmp(keyname[i], key_buf) == 0) {
      ev->key.keysym.sym = i;
      key_status[i] = (ev->type == SDL_KEYDOWN);
      return 1;
    }
  }
  return 0;
}

int SDL_WaitEvent(SDL_Event *ev) {
  while (!SDL_PollEvent(ev)) {
  }
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  (void)ev;
  (void)numevents;
  (void)action;
  (void)mask;
  return 0;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  if (numkeys != NULL) {
    *numkeys = (int)KEYNUM;
  }
  return key_status;
}
