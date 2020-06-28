#include "9cc.h"


void gen_lval(Node *node) {
  if (node->kind != ND_LVAR) {
    error("代入の左辺値が変数ではありません");
  }
  // rbp からのオフセットからローカル変数のアドレスを計算してスタックに積む
  printf("  # gen_lval start\n");
  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->offset);
  printf("  push rax\n");
  printf("  # gen_lval end\n");
}

int next_label_key() {
  static int label_index = 0;
  return label_index++;
}

void codegen(Node *node) {
  switch (node->kind) {
    case ND_NUM:
      printf("  push %d\n", node->val);
      return ;
    case ND_LVAR:
      printf("  # ND_LVAR start\n");
      gen_lval(node);
      printf("  pop rax\n"); // gen_lval(node)で積んだ変数のアドレスをraxにロード
      printf("  mov rax, [rax]\n"); // 変数のアドレスにある値をraxにロード
      printf("  push rax\n"); // 変数の値(rax)をスタックに積む
      printf("  # ND_LVAR end\n");
      return;
    case ND_ASSIGN:
      printf("  # ND_ASSIGN start\n");
      gen_lval(node->lhs);
      codegen(node->rhs);
      //rhsの結果がスタックの先頭、その次に変数のアドレスが入ってるのでそれをロード
      printf("  pop rdi\n"); // rhsの結果
      printf("  pop rax\n"); // 左辺の変数のアドレス
      printf("  mov [rax], rdi\n"); // 左辺の変数にrhsの結果を代入
      printf("  push rdi\n"); // この代入結果自体もスタックに積む(右結合でどんどん左に伝搬していくときの右辺値になる)
      printf("  # ND_ASSIGN end\n");
      return;
    case ND_RETURN:
      printf("  # ND_RETURN start\n");
      codegen(node->lhs);
      printf("  pop rax\n");
      printf("  mov rsp, rbp\n");
      printf("  pop rbp\n");
      printf("  ret\n");
      printf("  # ND_RETURN end\n");
      codegen(node->lhs);
      return;
    case ND_BLOCK:
      printf("  # ND_BLOCK start\n");
      // このブロックの先頭の文からコード生成
      for (int i = 0; node->code[i]; i++) {
        codegen(node->code[i]);
        // 最後に演算結果がスタックの先頭にあるので、スタックが溢れないようにそれを1文毎にraxに対比
        printf("  pop rax\n");
      }
      // このブロックの結果をスタックにpush
      printf("  push rax\n");
      printf("  # ND_BLOCK end\n");
      return;
    case ND_IF:
      {
        printf("  # ND_IF start\n");
        codegen(node->code[0]); // 条件式のコード生成
        if (node->code[2]) {
          // else ありの if
          printf("  pop rax\n"); // 条件式の結果をraxにロード
          printf("  cmp rax, 0\n"); // 条件式の結果チェック
          int else_label = next_label_key();
          int end_label = next_label_key();
          printf("  je .Lelse%04d\n", else_label); // false(rax == 0)ならwhile終了なのでジャンプ
          codegen(node->code[1]); // true節のコード生成
          printf("  jmp .Lend%04d\n", end_label); // true節のコードが終わったのでif文抜ける
          printf(".Lelse%04d:\n", else_label); // elseのときの飛崎
          codegen(node->code[2]); // false節のコード生成
          printf(".Lend%04d:\n", end_label); // elseのときの飛崎
        } else {
          // else なしの if
          printf("  pop rax\n"); // 条件式の結果をraxにロード
          printf("  cmp rax, 0\n"); // 条件式の結果チェック
          int end_label = next_label_key();
          printf("  je .Lend%04d\n", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
          codegen(node->code[1]); // true節のコード生成
          printf(".Lend%04d:\n", end_label); // elseのときの飛び先
        }
        printf("  # ND_IF end\n");
      }
      return;
    case ND_WHILE:
      {
        printf("  # ND_WHILE start\n");
        int begin_label = next_label_key();
        printf(".Lbegin%04d:\n", begin_label);
        printf("  # ND_WHILE condition start\n");
        codegen(node->lhs); // 条件式のコード生成
        printf("  # ND_WHILE condition end\n");
        printf("  pop rax\n"); // 条件式の結果をraxにロード
        printf("  cmp rax, 0\n"); // 条件式の結果チェック
        int end_label = next_label_key();
        printf("  je .Lend%04d\n", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
        printf("  # ND_WHILE body start\n");
        codegen(node->rhs); // whileの本体実行
        printf("  # ND_WHILE body end\n");
        printf("  jmp .Lbegin%04d\n", begin_label); //繰り返し
        printf(".Lend%04d:\n", end_label);
        printf("  # ND_WHILE end\n");
      }
      return;
    case ND_FOR:
      // node->code[0] ... 初期化式またはNULL
      // node->code[1] ... 条件式またはNULL
      // node->code[2] ... 継続式またはNULL
      // node->code[3] ... forの中身
      {
        printf("  # ND_FOR start\n");
        int begin_label = next_label_key();
        int end_label = next_label_key();
        // 初期化式
        if (node->code[0]) {
          codegen(node->code[0]);
        }
        printf(".Lbegin%04d:\n", begin_label);
        // 条件式
        if (node->code[1]) {
          codegen(node->code[1]);
          printf("  pop rax\n"); // 条件式の結果をraxにロード
          printf("  cmp rax, 0\n"); // 条件式の結果チェック
          printf("  je .Lend%04d\n", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
        }
        // for の中身のアセンブラ
        codegen(node->code[3]);
        // 継続式
        if (node->code[2]) {
          codegen(node->code[2]);
        }
        printf("  jmp .Lbegin%04d\n", begin_label); //繰り返し
        printf(".Lend%04d:\n", end_label);

        printf("  # ND_FOR end\n");
      }
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
