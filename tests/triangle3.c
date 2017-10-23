int main(void) {
 int i,j;
 int l = 0;
  for (i = 0; i < 10; i++) {
    for (j = 0; j < i*3 + 5; j += 2) { 
      l ++;
    } 
  }
  for (i = 0; i < l; i++);
}
