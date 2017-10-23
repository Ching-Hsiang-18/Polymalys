int main(void) {
 int i,j;
 int l = 0;
  for (i = 0; i < 10; i++) {
    for (j = 0; j < i; j ++) { 
      l ++;
    } 
  }
  for (i = 0; i < l; i++);
}
