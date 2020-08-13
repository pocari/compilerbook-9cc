[低レイヤを知りたい人のためのCコンパイラ作成入門](https://www.sigbus.info/compilerbook)

```
$ make
$ cat examples/fib.c
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
$ ./ynicc examples/fib.c > tmp.s
$ gcc -o tmp tmp.s
$ ./tmp
$ echo $?
8

$ cat examples/fizzbuzz.c
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
  int i;
  for (i = 1; i <= 20; i = i + 1) {
    fizz_buzz(i);
  }
}
$ ./ynicc examples/fizzbuzz.c > tmp.s
$ gcc -no-pie -otmp tmp.s
$ ./tmp
    1   1
    2   2
    3   Fizz
    4   4
    5   Buzz
    6   Fizz
    7   7
    8   8
    9   Fizz
   10   Buzz
   11   11
   12   Fizz
   13   13
   14   14
   15   FizzBuzz
   16   16
   17   17
   18   Fizz
   19   19
   20   Buzz

$ ./ynicc examples/sudoku.c > tmp.s
$ gcc -static -o sudoku tmp.s
$ ./sudoku
 . . . | . 7 . | . . .
 . 3 . | . . . | . 4 .
 . . . | 1 . . | . 8 .
 ------+-------+------
 1 . . | . . . | . . 6
 . . 9 | 3 . . | . . 5
 . 8 . | 2 . 9 | . . .
 ------+-------+------
 . . . | 6 . . | . . 2
 . . . | 4 1 . | . 3 .
 . 4 2 | . . 8 | 7 9 .

 2 9 4 | 8 7 5 | 1 6 3
 8 3 1 | 9 2 6 | 5 4 7
 5 6 7 | 1 4 3 | 2 8 9
 ------+-------+------
 1 5 3 | 7 8 4 | 9 2 6
 4 2 9 | 3 6 1 | 8 7 5
 7 8 6 | 2 5 9 | 3 1 4
 ------+-------+------
 3 1 8 | 6 9 7 | 4 5 2
 9 7 5 | 4 1 2 | 6 3 8
 6 4 2 | 5 3 8 | 7 9 1
```

