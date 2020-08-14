int printf();

int fib(int n) {
  if (n == 1) {
    return 1;
  } else if (n == 2) {
    return 1;
  } else {
    return fib(n - 1) + fib(n - 2);
  }
}

int main() {
  int x[11];

  for (int i = 1; i <= 10; i++) {
    x[i] = fib(i);
  }
  for (int i = 1; i <= 10; i++) {
    printf("x[%2d] ... %2d\n", i, x[i]);
  }

  return 0;
}
