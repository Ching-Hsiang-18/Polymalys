int add(int a, int b) {
  return a + b;
}
int main(void) {
  int a, i, b;
  b = a + 10;
  i = 0;
  while (add(a, i) < b) {
    i++;
  }
}
