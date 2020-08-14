int printf();

int fizz_buzz(int i) {
  if (i % 15 == 0) {
    printf("%5d\tFizzBuzz\n", i);
  } else if (i % 5 == 0) {
    printf("%5d\tBuzz\n", i);
  } else if (i % 3 == 0) {
    printf("%5d\tFizz\n", i);
  } else {
    printf("%5d\t%d\n", i, i);
  }
}

int main() {
  for (int i = 1; i <= 20; i++) {
    fizz_buzz(i);
  }
}

