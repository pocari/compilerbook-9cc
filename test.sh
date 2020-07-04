#!/bin/bash

counts=0
count_ok=0

assert() {
  expected="$1"
  input="$2"

  ./ynicc "$input" > tmp.s
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
assert 10 'main() {int a; a=10;}'
assert 11 'main() {int a; int b; a=10;b=a;b+1;}'
assert 20 'main() {int a; int b; a=b=10;a+b;}'
assert 15 'main() {int a; int b; a = 1 + 2; b = 3 * 4; a + b;}'
assert 15 'main() {int foo; int bar; foo = 1 + 2; bar = 3 * 4; foo + bar;}'
assert 1  'main() {int foo; int boo; int a; a=foo=1; boo = a + foo; boo == 2;}'

assert 2  'main() { return 2; }'
assert 4  'main() {int b; int boo; b=boo =1; return b + boo + 2; }'
assert 3  'main() { return 3; return 2;}'

assert 22 "$(cat <<EOS
main() {
  int a;
  int boo;  int hoge;
  a = 10;
  boo = 12;
  hoge = a + boo;
  return hoge;
}
EOS
)"

assert 10 "$(cat <<EOS
main() {
  int i;
  i = 0;
  while (i < 10)
    i = i + 1;
  return i;
}
EOS
)"

assert 30 "$(cat <<EOS
main() {
  int i;
  int sum;
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
  int foo;
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
  int foo;
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
  int foo;
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
  int foo;
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
  int sum;
  int i;
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
  int i;
  int sum;

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

assert 11 'main() {int returnx; returnx = 11; return returnx;}'

assert 45 "$(cat <<EOS
main() {
  int sum;
  int i;
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
  int sum;
  int i;
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
  int sum;
  int i;
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
  int foo;
  int bar;
  foo = 1;
  bar = 2;
  return foo_with_args_add(foo + bar, 3 + 4);
}
EOS
)"

assert 21 "main() { return foo_with_args_add6(1, 2, 3, 4, 5, 6);}";

assert 15 "$(cat <<EOS
hoge() {
  int foo;
  int boo;
  int baz;

  foo = 1;
  boo = 2;
  baz = 3;
  return foo + boo + baz;
}
main() {
  int foo;
  int bar;

  foo = 4;
  bar = 5;

  return hoge() + foo + bar;
}
EOS
)"

assert 10 "$(cat <<EOS
hoge(foo, boo) {
  int baz;
  baz = 4;
  return foo - boo - baz;
}
main() {
  int foo;
  int bar;
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


assert 1 "$(cat <<EOS
func(a, b) {
  int i;
  int j;
  int a;
  int b;
  for (i = 0; i < 10; i = i + 1) {
    a = a + 1;
  }
  j = 0;
  while (j < 10) {
    b = b + 1;
    j = j + 1;
  }
  if (a + b > 0) {
    return 1;
  } else {
    return 0;
  }
}

main() {
  return func(2, 3);
}
EOS
)"

assert 3 'main() {int foo; foo = 3; return *&foo;}'
assert 3 'main() {int foo; int boo; foo = 3; boo = &foo; return *boo;}'

# ローカル変数の持ち方として、最初に現れた変数ほどスタックの伸びる方向の端(アドレスの大きい方)にいる(=最初に出てくる変数がローカル変数のアドレスとしては一番大きくなる)にある
# main() {a = 1; b = 2; c = 3; }
# だと、スタック上のレイアウトとしては(変数が8バイトとして)
# addr    変数
# 100 ... c
#  92 ... b
#  84 ... a
#   |
#   v スタック伸びる方向
#
# なので、
# b のアドレスは aのアドレスからみて +8
# b のアドレスは cのアドレスからみて -8
assert 5 'main() {int a; int b;int c;int d; a = 4; b = 5; c = 6; d = &a + 8; return *d; }'
assert 5 'main() {int a; int b; int c; int d; a = 4; b = 5; c = 6; d = &c - 8; return *d; }'
assert 12 'main() {int a; int b; int c; a = 4; b = 5; c = 6; *(&c - 8) = 12; return b; }'

assert 3 'main() {int x; x=3; return *&x; }'
assert 3 'main() {int x; int y; int z;x=3; y=&x; z=&y; return **z; }'
assert 5 'main() {int x; int y; x=3; y=5; return *(&x+8); }'
assert 3 'main() {int x; int y; x=3; y=5; return *(&y-8); }'
assert 5 'main() {int x; int y; x=3; y=&x; *y=5; return x; }'
assert 7 'main() {int x; int y; x=3; y=5; *(&x+8)=7; return y; }'
assert 7 'main() {int x; int y; x=3; y=5; *(&y-8)=7; return x; }'



echo "---------------------------------"
echo "total case: $count, ok: $count_ok"

if [ $count = $count_ok ]; then
  exit 0
else
  exit 1
fi
