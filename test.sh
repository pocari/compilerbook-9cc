#!/bin/bash

counts=0
count_ok=0

assert() {
  expected="$1"
  input="$2"

  ./ynicc "$input" > tmp.s
  # 関数呼び出しのテスト用にgcc側で正常な関数を作ってリンクする
  gcc -c test_func.c
  
  # -no-pie オプションをつけないと、グローバル変数が存在する場合になぜか
  # /usr/bin/ld: /tmp/ccMACvL0.o: relocation R_X86_64_32S against `.data' can not be used when making a PIE object; recompile with -fPIE
  # のエラーになる。
  gcc -no-pie -o tmp test_func.o tmp.s
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

assert 0  'int main() { return 0; }'
assert 42 'int main() { return 42; }'
assert 21 'int main() { return 5+20-4;}'
assert 41 'int main() { return  12 + 34 - 5 ;}'
assert 47 'int main() { return 5+6*7;}'
assert 15 'int main() { return 5 * (9- 6);}'
assert 4  'int main() { return ( 3 + 5) / 2;}'
assert 60 'int main() { return (1 + 2) * 3 + 4 + (5 + 6 * 7);}'
assert 5  'int main() { return -1 + 2 * 3;}'
assert 5  'int main() { return -1 + +2 * 3;}'
assert 12 'int main() { return -(-1 + -2) * 4;}'
assert 0  'int main() { return 0==1;}'
assert 1  'int main() { return 42==42;}'
assert 1  'int main() { return 0!=1;}'
assert 0  'int main() { return 42!=42; }'
assert 1  'int main() { return 0<1;}'
assert 0  'int main() { return 1<1;}'
assert 0  'int main() { return 2<1;}'
assert 1  'int main() { return 0<=1;}'
assert 1  'int main() { return 1<=1;}'
assert 0  'int main() { return 2<=1;}'
assert 1  'int main() { return 1>0;}'
assert 0  'int main() { return 1>1;}'
assert 0  'int main() { return 1>2;}'
assert 1  'int main() { return 1>=0;}'
assert 1  'int main() { return 1>=1;}'
assert 0  'int main() { return 1>=2;}'
assert 10 'int main() {int a; return a=10;}'
assert 11 'int main() {int a; int b; a=10;b=a; return b+1;}'
assert 20 'int main() {int a; int b; a=b=10; return a+b;}'
assert 15 'int main() {int a; int b; a = 1 + 2; b = 3 * 4; return a + b;}'
assert 15 'int main() {int foo; int bar; foo = 1 + 2; bar = 3 * 4; return foo + bar;}'
assert 1  'int main() {int foo; int boo; int a; a=foo=1; boo = a + foo; return boo == 2;}'
assert 2  'int main() { return 2; }'
assert 4  'int main() {int b; int boo; b=boo =1; return b + boo + 2; }'
assert 3  'int main() { return 3; return 2;}'

assert 22 "$(cat <<EOS
int main() {
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
int main() {
  int i;
  i = 0;
  while (i < 10)
    i = i + 1;
  return i;
}
EOS
)"

assert 30 "$(cat <<EOS
int main() {
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
int main() {
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
int main() {
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
int main() {
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
int main() {
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
int main() {
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
int main() {
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

assert 11 'int main() {int returnx; returnx = 11; return returnx;}'

assert 45 "$(cat <<EOS
int main() {
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
int main() {
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
int main() {
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
assert 2 'int main() { return foo_return2(); }'
assert 4 'int main() { return 2 + foo_return2();}'

assert 5 'int main() { return foo_with_args_add(2, 3);}'
assert 10 "$(cat <<EOS
int main() {
  int foo;
  int bar;
  foo = 1;
  bar = 2;
  return foo_with_args_add(foo + bar, 3 + 4);
}
EOS
)"

assert 21 "int main() { return foo_with_args_add6(1, 2, 3, 4, 5, 6);}";

assert 15 "$(cat <<EOS
int hoge() {
  int foo;
  int boo;
  int baz;

  foo = 1;
  boo = 2;
  baz = 3;
  return foo + boo + baz;
}
int main() {
  int foo;
  int bar;

  foo = 4;
  bar = 5;

  return hoge() + foo + bar;
}
EOS
)"

assert 10 "$(cat <<EOS
int hoge(int foo, int boo) {
  int baz;
  baz = 4;
  return foo - boo - baz;
}
int main() {
  int foo;
  int bar;
  foo = 1;
  bar = 2;

  return hoge(20, 3) - foo - bar;
}
EOS
)"

assert 120 "$(cat <<EOS
int fact(int n) {
  if (n == 1) {
    return 1;
  } else {
    return fact(n - 1) * n;
  }
}

int main() {
  return fact(5);
}
EOS
)"

assert 55 "$(cat <<EOS
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
  return fib(10);
}
EOS
)"


assert 1 "$(cat <<EOS
int func(int a, int b) {
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

int main() {
  return func(2, 3);
}
EOS
)"

assert 3 'int main() {int foo; foo = 3; return *&foo;}'
assert 3 'int main() {int foo; int boo; foo = 3; boo = &foo; return *boo;}'

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
# b のアドレスは aのアドレスからみて +8なので、8バイトのポインタ演算的には+1
# b のアドレスは cのアドレスからみて -8なので、8バイトのポインタ演算的には-1
assert 5 'int main() {int a; int b;int c;int d; a = 4; b = 5; c = 6; d = &a + 1; return *d; }'
assert 5 'int main() {int a; int b; int c; int d; a = 4; b = 5; c = 6; d = &c - 1; return *d; }'
assert 12 'int main() {int a; int b; int c; a = 4; b = 5; c = 6; *(&c - 1) = 12; return b; }'

assert 3 'int main() {int x; x=3; return *&x; }'
assert 3 'int main() {int x; int y; int z;x=3; y=&x; z=&y; return **z; }'
assert 5 'int main() {int x; int y; x=3; y=5; return *(&x+1); }'
assert 3 'int main() {int x; int y; x=3; y=5; return *(&y-1); }'
assert 5 'int main() {int x; int y; x=3; y=&x; *y=5; return x; }'
assert 7 'int main() {int x; int y; x=3; y=5; *(&x+1)=7; return y; }'
assert 7 'int main() {int x; int y; x=3; y=5; *(&y-1)=7; return x; }'

assert 23 "$(cat <<EOS
int main() {
  int x;
  int *y;
  int **z;

  x = 12;
  y = &x;
  z = &y;

  **z = 23;

  return x;
}
EOS
)"

# num + ptrのテスト
assert 3 "int main() { int *p; alloc_3num_ary_8_byte_cell(&p); return *(p + 1);}"
# ptr + numのテスト
assert 4 "int main() { int *p; alloc_3num_ary_8_byte_cell(&p); return *(2 + p);}"
# ptr - ptrのテスト
assert 3 "int main() { int *p; int *q; alloc_3num_ary_8_byte_cell(&p); q = p + 3; return q - p;}"
# ptr - numのテスト
assert 4 "int main() { int *p; int *q; alloc_3num_ary_8_byte_cell(&p); q = p + 3; return *(q - 1);}"

assert 8  'int main() { return sizeof(1); }'
assert 8  'int main() { int x; return sizeof(x); }'
assert 8  'int main() { int x; return sizeof(&x); }'
assert 8  'int main() { int *x; return sizeof(x); }'
assert 8  'int main() { return sizeof(1 + 2); }'
assert 8  'int main() { int **x; return sizeof x; }'
assert 8  'int main() { return sizeof(sizeof(0)); }'
assert 8  'int main() { int x; return sizeof sizeof &x; }'

assert 3 "$(cat <<EOS
int main() {
  int a[2];
  *a = 1;
  *(a + 1) = 2;
  int *p;
  p = a;
  return *p + *(p + 1);
}
EOS
)"

assert 6 "$(cat <<EOS
int main() {
  int a[2];
  a[0] = 1;
  a[1] = 2;
  int *p;
  p = a;
  return *p + *(p + 1) + a[1 - 1] + a[0 + 1];
}
EOS
)"

# cでは、 a[3]も3[a]も同じ意味らしい
assert 6 "$(cat <<EOS
int hoge() { return 2; }
int main() {
  int a[2];
  a[0] = 1;
  a[1] = 2;
  int *p;
  p = a;
  return *p + *(p + 1) + 0[a] + (-1 + hoge())[a];
}
EOS
)"

assert 0 'int main() { int x[2][3]; int *y; y = x; *y=0; return **x; }'
assert 1 'int main() { int x[2][3]; int *y; y = x; *(y+1)=1; return *(*x+1); }'
assert 2 'int main() { int x[2][3]; int *y; y = x; *(y+2)=2; return *(*x+2); }'
assert 3 'int main() { int x[2][3]; int *y; y = x; *(y+3)=3; return **(x+1); }'
assert 4 'int main() { int x[2][3]; int *y; y = x; *(y+4)=4; return *(*(x+1)+1); }'
assert 5 'int main() { int x[2][3]; int *y; y = x; *(y+5)=5; return *(*(x+1)+2); }'
assert 6 'int main() { int x[2][3]; int *y; y = x; *(y+6)=6; return **(x+2); }'

assert 21 "$(cat <<EOS
int main() {
  int a[2][3];
  int *p;

  a[0][0] = 1;
  a[0][1] = 2;
  a[0][2] = 3;
  a[1][0] = 4;
  a[1][1] = 5;
  a[1][2] = 6;

  p = a + 1;

  return a[0][0] + a[0][1] + a[0][2] + *p + *(p + 1) + *(p + 2);
}
EOS
)"

assert 0 'int x; int main() { return x; }'
assert 5 'int x; int func(int num) { x = num; } int main() { func(5); return x; }'
assert 6 'int x[3]; int main() { x[0] = 1; x[1] = 2; x[2] = 3; int *p; p = x; return *p + *(p + 1) + *(p + 2); }'
assert 12 'int *x; int main() { int y; y = 12; x = &y; return *x; }'

assert 1 'int main() { char x; return sizeof(x); }'
assert 3 "$(cat <<EOS
int main() {
  char x[3];
  x[0] = -1;
  x[1] = 2;
  int y;
  y = 4;
  return x[0] + y;
}
EOS
)"

assert 5 'char x[2]; int main() { x[0] = 2; x[1] = 3; return x[0] + x[1]; }'

assert 1 'int main() { char x; x=1; return x; }'
assert 1 'int main() { char x; x=1; char y; y=2; return x; }'
assert 2 'int main() { char x; x=1; char y; y=2; return y; }'

assert 1 'int main() { char x; return sizeof(x); }'
assert 10 'int main() { char x[10]; return sizeof(x); }'
assert 1 'int main() { return sub_char(7, 3, 3); } int sub_char(char a, char b, char c) { return a-b-c; }'


echo "---------------------------------"
echo "total case: $count, ok: $count_ok"

if [ $count = $count_ok ]; then
  exit 0
else
  exit 1
fi
