#define MAXSIZE 10
int main(void) {
  int base;
  int end;
  int i;
  int k;
  if (end - base > MAXSIZE)
    end = base + MAXSIZE;
  for (i = base; i < end; i++); 
  
}