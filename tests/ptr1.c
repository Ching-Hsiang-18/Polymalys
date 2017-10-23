int main(void) {
  int i;
  int *ptr;
  int j = 10;
  ptr = &j;
  (*ptr) += 5;
  for (i = 0; i < j; i++) {
  }
}
