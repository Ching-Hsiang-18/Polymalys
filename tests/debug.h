#define MY_ASSERT(e) {\
  *((int*)0x43) = (e) - 1; \
}

#define VIEW(e) {\
  *((int*)0x40) = e; \
  *((int*)0x42) = 1; \
}


#define PRINT_STATE {\
  *((int*)0x42) = 1; \
}
#define PROVE MY_ASSERT
#define STATIC_ASSERT MY_ASSERT

static inline char getRand(char a, char b) {
  char c;
  if (c < a) c = a;
  if (c > b) c = b;
  return c;
}
