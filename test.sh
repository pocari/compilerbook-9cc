#!/bin/bash

assert() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  # 関数呼び出しのテスト用にgcc側で正常な関数を作ってリンクする
  gcc -c test_func.c
  cc -o tmp test_func.o tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
  fi
}

assert 0 '0;'
assert 42 '42;'
assert 21 '5+20-4;'
assert 41 ' 12 + 34 - 5 ;'
assert 47 '5+6*7;'
assert 15 '5 * (9- 6);'
assert 4 '( 3 + 5) / 2;'
assert 60 '(1 + 2) * 3 + 4 + (5 + 6 * 7);'
assert 5 '-1 + 2 * 3;'
assert 5 '-1 + +2 * 3;'
assert 12 '-(-1 + -2) * 4;'
assert 0 '0==1;'
assert 1 '42==42;'
assert 1 '0!=1;'
assert 0 '42!=42;'

assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'

assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'

assert 10 'a=10;'
assert 11 'a=10;b=a;b+1;'
assert 20 'a=b=10;a+b;'
assert 15 'a = 1 + 2; b = 3 * 4; a + b;'
assert 15 'foo = 1 + 2; bar = 3 * 4; foo + bar;'
assert 1 'a=foo=1; boo = a + foo; boo == 2;'

assert 2 'return 2;'
assert 4 'b=boo =1; return b + boo + 2;'
assert 3 'return 3; return 2;'

assert 22 "$(cat <<EOS
a = 10;
boo = 12;
hoge = a + boo;
return hoge;
EOS
)"

assert 10 "$(cat <<EOS
i = 0;
while (i < 10)
  i = i + 1;
return i;
EOS
)"

assert 30 "$(cat <<EOS
i = 0;
sum = 0;
while (i < 10) {
  sum = sum + 3;
  i = i + 1;
}
return sum;
EOS
)"

assert 11 "$(cat <<EOS
foo = 1;
if (foo == 1) {
  return 11;
}
return 12;
EOS
)"

assert 12 "$(cat <<EOS
foo = 0;
if (foo == 1) {
  return 11;
}
return 12;
EOS
)"

assert 13 "$(cat <<EOS
foo = 0;
if (foo == 1) {
  return 11;
} else {
  return 13;
}
return 12;
EOS
)"

assert 11 "$(cat <<EOS
foo = 1;
if (foo == 1) {
  return 11;
} else {
  return 13;
}
return 12;
EOS
)"

assert 4 "$(cat <<EOS
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
EOS
)"

assert 5 "$(cat <<EOS
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
EOS
)"

assert 11 'returnx = 11; return returnx;'

assert 45 "$(cat <<EOS
sum = 0;
for (i = 0; i < 10; i = i + 1) {
  sum = sum + i;
}
return sum;
EOS
)"

assert 45 "$(cat <<EOS
sum = 0;
i = 0;
for (; i < 10; i = i + 1) {
  sum = sum + i;
}
return sum;
EOS
)"

assert 45 "$(cat <<EOS
sum = 0;
i = 0;
for (; i < 10;) {
  sum = sum + i;
  i = i + 1;
}
return sum;
EOS
)"

# 一旦今の時点では 別途gccでコンパイルした test_func.cで定義した foo_return2 を使う
assert 2 "return foo_return2();"
assert 4 "return 2 + foo_return2();"

assert 5 "return foo_with_args_add(2, 3);";
assert 10 "$(cat <<EOS
foo = 1;
bar = 2;
return foo_with_args_add(foo + bar, 3 + 4);
EOS
)"

assert 21 "return foo_with_args_add6(1, 2, 3, 4, 5, 6);";
