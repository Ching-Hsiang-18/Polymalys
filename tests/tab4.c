#include "debug.h"
int main(void) {
  int tab[10];
  int i, a, b;
  for (i = 0; i < 10; i++)
    tab[i] = 42;
  if ((a >= 0) && (a < 10)) {
    if ((b >= 0) && (b < 10)) {
      tab[a] = 5;
      tab[b] = 6;
      /*
      STATIC_ASSERT(tab[a] == 10);
      STATIC_ASSERT(tab[b] == 10);
      STATIC_ASSERT(tab[3] >= 10 && tab[3] <= 42);
      */
      for (i = 0; i < tab[a]; i++);
      for (i = 0; i < tab[b]; i++);
      for (i = 0; i < tab[3]; i++);
      
    }
  }
  
}
