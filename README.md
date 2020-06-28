[低レイヤを知りたい人のためのCコンパイラ作成入門](https://www.sigbus.info/compilerbook)

```
make
cat examples/fib.c
./9cc "$(cat examples/fib.c)" > tmp.s
gcc -o tmp tmp.s
./tmp
echo $?
```

