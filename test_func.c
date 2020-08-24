#include <stdio.h>
#include <stdlib.h>

// extern のテストで使う
int f84_ext1;
int *f84_ext2;

// テストのときに使う関数
int foo_return2() {
  return 2;
}

int foo_with_args_add(int a, int b) {
  return a + b;
}

int foo_with_args_add6(
    int a1,
    int a2,
    int a3,
    int a4,
    int a5,
    int a6
) {
  return a1 + a2 + a3 + a4 + a5 + a6;
}

void dump_address(void *p) {
  fprintf(stderr, "p ... %p\n", p);
}

void alloc_3num_ary_8_byte_cell(int **p) {
  int *lp = *p = (void *)calloc(4, 8);

  lp[0] = 2;
  lp[1] = 3;
  lp[2] = 4;
  lp[3] = 5;
}


// // テストのテスト用main
// int main() {
//   long *p;
// 
//   alloc_3num_ary_8_byte_cell(&p, 12, 13, 14, 15);
//   printf("p[0]: %ld\n", p[0]);
//   printf("p[1]: %ld\n", p[1]);
//   printf("p[2]: %ld\n", p[2]);
// 
//   printf("sizeof(int): %ld\n", sizeof(int));
//   printf("sizeof(long): %ld\n", sizeof(long));
//   return 0;
// }

