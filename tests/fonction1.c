void inc(int *p) {
  (*p) ++;
}

int main(void) {
  int i = 0;
  int b = 0;
  while (i < 10) {
    inc(&i);
    b++;
  }
  for (i = 0; i < b; i++);
}
