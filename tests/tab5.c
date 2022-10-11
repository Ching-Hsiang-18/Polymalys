int main(void) {
  int tab[25];
  int i, j;
  for (i = 10; i < 15;) {
    for (j = 0; j < 5; j++) {
      tab[(i-10)*5 + j] = 42;
    }
    i++;
 }  
}
