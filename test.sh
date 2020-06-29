#!/bin/bash

counts=0
count_ok=0

assert() { expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  # 関数呼び出しのテスト用にgcc側で正常な関数を作ってリンクする
  gcc -c test_func.c
  cc -o tmp test_func.o tmp.s
  ./tmp
  actual="$?"

  count=$((count+1))
  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
    count_ok=$((count_ok+1))
  else
    echo "$input => $expected expected, but got $actual"
  fi
}

assert 0  'main() { 0; }'
assert 42 'main() { 42; }'
assert 21 'main() { 5+20-4;}'
assert 41 'main() {  12 + 34 - 5 ;}'
assert 47 'main() { 5+6*7;}'
assert 15 'main() { 5 * (9- 6);}'
assert 4  'main() { ( 3 + 5) / 2;}'
assert 60 'main() { (1 + 2) * 3 + 4 + (5 + 6 * 7);}'
assert 5  'main() { -1 + 2 * 3;}'
assert 5  'main() { -1 + +2 * 3;}'
assert 12 'main() { -(-1 + -2) * 4;}'
assert 0  'main() { 0==1;}'
assert 1  'main() { 42==42;}'
assert 1  'main() { 0!=1;}'
assert 0  'main() { 42!=42; }'

assert 1  'main() { 0<1;}'
assert 0  'main() { 1<1;}'
assert 0  'main() { 2<1;}'
assert 1  'main() { 0<=1;}'
assert 1  'main() { 1<=1;}'
assert 0  'main() { 2<=1;}'
assert 1  'main() { 1>0;}'
assert 0  'main() { 1>1;}'
assert 0  'main() { 1>2;}'
assert 1  'main() { 1>=0;}'
assert 1  'main() { 1>=1;}'
assert 0  'main() { 1>=2;}'
assert 10 'main() { a=10;}'
assert 11 'main() { a=10;b=a;b+1;}'
assert 20 'main() { a=b=10;a+b;}'
assert 15 'main() { a = 1 + 2; b = 3 * 4; a + b;}'
assert 15 'main() { foo = 1 + 2; bar = 3 * 4; foo + bar;}'
assert 1  'main() { a=foo=1; boo = a + foo; boo == 2;}'

assert 2  'main() { return 2; }'
assert 4  'main() { b=boo =1; return b + boo + 2; }'
assert 3  'main() { return 3; return 2;}'

assert 22 "$(cat <<EOS
main() {
  a = 10;
  boo = 12;
  hoge = a + boo;
  return hoge;
}
EOS
)"

assert 10 "$(cat <<EOS
main() {
  i = 0;
  while (i < 10)
    i = i + 1;
  return i;
}
EOS
)"

assert 30 "$(cat <<EOS
main() {
  i = 0;
  sum = 0;
  while (i < 10) {
    sum = sum + 3;
    i = i + 1;
  }
  return sum;
}
EOS
)"

assert 11 "$(cat <<EOS
main() {
  foo = 1;
  if (foo == 1) {
    return 11;
  }
  return 12;
}
EOS
)"

assert 12 "$(cat <<EOS
main() {
  foo = 0;
  if (foo == 1) {
    return 11;
  }
  return 12;
}
EOS
)"

assert 13 "$(cat <<EOS
main() {
  foo = 0;
  if (foo == 1) {
    return 11;
  } else {
    return 13;
  }
  return 12;
}
EOS
)"

assert 11 "$(cat <<EOS
main() {
  foo = 1;
  if (foo == 1) {
    return 11;
  } else {
    return 13;
  }
  return 12;
}
EOS
)"

assert 4 "$(cat <<EOS
main() {
  i = 0;
  sum = 0;
  while (i < 10) {
    sum = sum + 2;
    i = i + 1;
  }
  if (sum == 20) {
    return 4;
  } else {
    return 5;
  }
}
EOS
)"

assert 5 "$(cat <<EOS
main() {
  i = 0;
  sum = 0;
  while (i < 10) {
    sum = sum + 2;
    i = i + 1;
  }
  if (sum != 20) {
    return 4;
  } else {
    return 5;
  }
}
EOS
)"

assert 11 'main() {returnx = 11; return returnx;}'

assert 45 "$(cat <<EOS
main() {
  sum = 0;
  for (i = 0; i < 10; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}
EOS
)"

assert 45 "$(cat <<EOS
main() {
  sum = 0;
  i = 0;
  for (; i < 10; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}
EOS
)"

assert 45 "$(cat <<EOS
main() {
  sum = 0;
  i = 0;
  for (; i < 10;) {
    sum = sum + i;
    i = i + 1;
  }
  return sum;
}
EOS
)"

# 一旦今の時点では 別途gccでコンパイルした test_func.cで定義した foo_return2 を使う
assert 2 'main() { return foo_return2(); }'
assert 4 'main() { return 2 + foo_return2();}'

assert 5 'main() { return foo_with_args_add(2, 3);}'
assert 10 "$(cat <<EOS
main() {
  foo = 1;
  bar = 2;
  return foo_with_args_add(foo + bar, 3 + 4);
}
EOS
)"

assert 21 "main() { return foo_with_args_add6(1, 2, 3, 4, 5, 6);}";

assert 15 "$(cat <<EOS
hoge() {
  foo = 1;
  boo = 2;
  baz = 3;
  return foo + boo + baz;
}
main() {
  foo = 4;
  bar = 5;

  return hoge() + foo + bar;
}
EOS
)"

assert 10 "$(cat <<EOS
hoge(foo, boo) {
  baz = 4;
  return foo - boo - baz;
}
main() {
  foo = 1;
  bar = 2;

  return hoge(20, 3) - foo - bar;
}
EOS
)"

assert 120 "$(cat <<EOS
fact(n) {
  if (n == 1) {
    return 1;
  } else {
    return fact(n - 1) * n;
  }
}

main() {
  return fact(5);
}
EOS
)"

assert 55 "$(cat <<EOS
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
EOS
)"

echo "---------------------------------"
echo "total case: $count, ok: $count_ok"

if [ $count = $count_ok ]; then
  exit 0
else
  exit 1
fi
