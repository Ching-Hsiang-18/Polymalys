int bound = 42;
void foo() {
  bound--;
}
int main(int cond) {
  int i;
  int *ptr = &bound;
  if (cond) {
    (*ptr) += 10;
  }
  for (i = 0; i < bound; i++) {
    foo();
  }  
}
