struct st {
  int a;
  int b;
};

int main(void) {
  struct st st[10];
  int i;
  for (i = 0; i < 10; i++) {
    st[i].a = 42;
  }  
  for (i = 0; i < 10; i++) {
    st[i].b = 51;
  }
  
  for (i = 0; i < st[1].a; i++);
  for (i = 0; i < st[1].b; i++);
}
