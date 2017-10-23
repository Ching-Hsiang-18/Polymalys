int main(void) {
  int i;
  int *ptr;
  ptr = &i;
  for (i = 0; i < 10; i += 2) {
    (*ptr)--;
  }
}
