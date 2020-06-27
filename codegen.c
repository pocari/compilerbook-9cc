#include "9cc.h"

void gen_lval(Node *node) {
  if (node->kind != ND_LVAR) {
    error("代入の左辺値が変数ではありません");
  }

  // 識別子 a 〜 z まであり、それぞれrbpから8, 16, 32 ...のオフセットのところにあるので、
  // その識別子に対応する場所のアドレスをスタックに積む
  // スタックが伸びる方向は↓とする
  //  | ... |
  //  +-----+
  //  |     | <- rbp (この関数のスタックのベースポインタ)
  //  +-----+
  //  |     | <- 識別子 a (rbp - 8)
  //  +-----+
  //  |     | <- 識別子 b (rbp - 16)
  //  +-----+
  //  |     |
  //  +-----+
  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->offset);
  printf("  push rax\n");
}

void codegen(Node *node) {
  switch (node->kind) {
    case ND_NUM:
      printf("  push %d\n", node->val);
      return ;
    case ND_LVAR:
      gen_lval(node);
      printf("  pop rax\n"); // gen_lval(node)で積んだ変数のアドレスをraxにロード
      printf("  mov rax, [rax]\n"); // 変数のアドレスにある値をraxにロード
      printf("  push rax\n"); // 変数の値(rax)をスタックに積む
      return;
    case ND_ASSIGN:
      gen_lval(node->lhs);
      codegen(node->rhs);
      //rhsの結果がスタックの先頭、その次に変数のアドレスが入ってるのでそれをロード
      printf("  pop rdi\n"); // rhsの結果
      printf("  pop rax\n"); // 左辺の変数のアドレス
      printf("  mov [rax], rdi\n"); // 左辺の変数にrhsの結果を代入
      printf("  push rdi\n"); // この代入結果自体もスタックに積む(右結合でどんどん左に伝搬していくときの右辺値になる)
      return;
  }

  codegen(node->lhs);
  codegen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rdi\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LTE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_EQL:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_NOT_EQL:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
      break;
    default:
      error("予期しないNodeです。 kind: %d\n", node->kind);
  }
  printf("  push rax\n");
}
