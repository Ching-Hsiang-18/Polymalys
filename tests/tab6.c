#include "debug.h"
int main(void) {
   int tab[10];
   int i, a, b;
   int k = 0;
   
   if ((a >= 0) && (b < 10) && (b > a)) {
     for (i=a; i < b;  i++) {
             tab[i]=123;
     }
    k = tab[a];
//    MY_ASSERT(tab[a] == 124);
   }
   for (i = 0; i < k; i++) {
   }
}
