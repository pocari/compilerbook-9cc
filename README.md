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
$ ./ynicc "$(cat examples/fib.c)" > tmp.s
$ gcc -o tmp tmp.s
$ ./tmp
$ echo $?
8

$ ./ynicc "$(cat examples/fizzbuzz.c)" > tmp.s
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
```

