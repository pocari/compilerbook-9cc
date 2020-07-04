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
  int x;
  x = 1 + 9;
  return fib(x);
}
$ ./ynicc "$(cat examples/fib.c)" > tmp.s
$ gcc -o tmp tmp.s
$ ./tmp
$ echo $?
55
```

