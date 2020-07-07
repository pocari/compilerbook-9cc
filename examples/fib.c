int fib(int n) {
  if (n == 1) {
    return 1;
  } else {
    if (n == 2) {
      return 1;
    } else {
      return fib(n - 1) + fib(n - 2);
    }
  }
}

int main() {
  int x[10];
  int i;

  for (i = 1; i <= 10; i = i + 1) {
    *(x + i - 1) = fib(i);
  }
  return *(x + 5);
}
