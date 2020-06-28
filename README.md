[低レイヤを知りたい人のためのCコンパイラ作成入門](https://www.sigbus.info/compilerbook)

```
$ make
$ cat examples/fib.c
fib(n) {
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

main() {
  return fib(10);
}
$ ./9cc "$(cat examples/fib.c)" > tmp.s
$ gcc -o tmp tmp.s
$ ./tmp
$ echo $?
55
```

