int foo(void) {
  int i,j;
  
  int k = 0;
  for (i = 0; i < 10; i++) {
    for (j = 0; j < i; j++) {
      k++;
    }
  }
  for (i = 0; i < k; i++);
  
}

int main(void) {
  return foo();
}