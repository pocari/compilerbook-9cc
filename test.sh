#!/bin/bash

assert() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  cc -o tmp tmp.s
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

