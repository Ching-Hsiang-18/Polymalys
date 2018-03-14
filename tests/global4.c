int bound;
void foo() {
  bound--;
}
int main(int cond) {
  bound = 42;
  int i;
  int *ptr = &bound;
  if (cond) {
    (*ptr) += 10;
  }
  for (i = 0; i < bound; i++) {
    foo();
  }  
}
