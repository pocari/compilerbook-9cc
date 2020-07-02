#include "ynicc.h"

void gen(Node *node);

void gen_addr(Node *node) {
  // switchの警告を消すpragma
  #pragma clang diagnostic ignored "-Wswitch"
  switch(node->kind) {
    case ND_LVAR:
      printf("  # gen_addr start (var_name: %s)\n", node->var->name);
      printf("  # gen_addr-ND_LVAR start\n");
      printf("  mov rax, rbp\n");
      printf("  sub rax, %d\n", node->var->offset);
      printf("  push rax\n");
      printf("  # gen_addr-ND_LVAR end\n");
      printf("  # gen_addr end\n");
      return;
    case ND_DEREF:
      printf("  # gen_addr start (var_name: none)\n");
      printf("  # gen_addr-ND_DEREF start\n");
      gen(node->lhs);
      printf("  # gen_addr-ND_DEREF stop\n");
      printf("  # gen_addr end\n");
      return;
  }

 error("代入の左辺値が変数ではありません");
}

int next_label_key() {
  static int label_index = 0;
  return label_index++;
}

// 引数に渡す時用のレジスタ
// ND_CALLの実装参照
static char *ARGUMENT_REGISTERS[] = {
  "rdi",
  "rsi",
  "rdx",
  "rcx",
  "r8",
  "r9",
};

void gen(Node *node) {
  // switchの警告を消すpragma
  #pragma clang diagnostic ignored "-Wswitch"
  switch (node->kind) {
    case ND_NUM:
      printf("  push %d\n", node->val);
      return ;
    case ND_LVAR:
      if (node->var) {
        printf("  # ND_LVAR start(var_name: %s)\n", node->var->name);
      } else {
        printf("  # ND_LVAR start(var_name: none)\n");
      }
      gen_addr(node);
      printf("  pop rax\n"); // gen_addr(node)で積んだ変数のアドレスをraxにロード
      printf("  mov rax, [rax]\n"); // 変数のアドレスにある値をraxにロード
      printf("  push rax\n"); // 変数の値(rax)をスタックに積む
      printf("  # ND_LVAR end\n");
      return;
    case ND_ASSIGN:
      printf("  # ND_ASSIGN start\n");
      gen_addr(node->lhs);
      gen(node->rhs);
      //rhsの結果がスタックの先頭、その次に変数のアドレスが入ってるのでそれをロード
      printf("  pop rdi\n"); // rhsの結果
      printf("  pop rax\n"); // 左辺の変数のアドレス
      printf("  mov [rax], rdi\n"); // 左辺の変数にrhsの結果を代入
      printf("  push rdi\n"); // この代入結果自体もスタックに積む(右結合でどんどん左に伝搬していくときの右辺値になる)
      printf("  # ND_ASSIGN end\n");
      return;
    case ND_RETURN:
      printf("  # ND_RETURN start\n");
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  mov rsp, rbp\n");
      printf("  pop rbp\n");
      printf("  ret\n");
      printf("  # ND_RETURN end\n");
      return;
    case ND_BLOCK:
      printf("  # ND_BLOCK start\n");
      // このブロックの先頭の文からコード生成
      //
      for (Node *n = node->body; n; n = n->next) {
        gen(n);
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
        gen(node->cond); // 条件式のコード生成
        if (node->els) {
          // else ありの if
          printf("  pop rax\n"); // 条件式の結果をraxにロード
          printf("  cmp rax, 0\n"); // 条件式の結果チェック
          int else_label = next_label_key();
          int end_label = next_label_key();
          printf("  je .L.else.%04d\n", else_label); // false(rax == 0)ならwhile終了なのでジャンプ
          gen(node->then);                         // true節のコード生成
          printf("  jmp .L.end.%04d\n", end_label); // true節のコードが終わったのでif文抜ける
          printf(".L.else.%04d:\n", else_label); // elseのときの飛崎
          gen(node->els);                      // false節のコード生成
          printf(".L.end.%04d:\n", end_label); // elseのときの飛崎
        } else {
          // else なしの if
          printf("  pop rax\n"); // 条件式の結果をraxにロード
          printf("  cmp rax, 0\n"); // 条件式の結果チェック
          int end_label = next_label_key();
          printf("  je .L.end.%04d\n", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
          gen(node->then);                       // true節のコード生成
          printf(".L.end.%04d:\n", end_label); // elseのときの飛び先
        }
        printf("  # ND_IF end\n");
      }
      return;
    case ND_WHILE:
      {
        printf("  # ND_WHILE start\n");
        int begin_label = next_label_key();
        printf(".L.begin.%04d:\n", begin_label);
        printf("  # ND_WHILE condition start\n");
        gen(node->cond); // 条件式のコード生成
        printf("  # ND_WHILE condition end\n");
        printf("  pop rax\n"); // 条件式の結果をraxにロード
        printf("  cmp rax, 0\n"); // 条件式の結果チェック
        int end_label = next_label_key();
        printf("  je .L.end.%04d\n", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
        printf("  # ND_WHILE body start\n");
        gen(node->body); // whileの本体実行
        printf("  # ND_WHILE body end\n");
        printf("  jmp .L.begin.%04d\n", begin_label); //繰り返し
        printf(".L.end.%04d:\n", end_label);
        printf("  # ND_WHILE end\n");
      }
      return;
    case ND_FOR:
      {
        printf("  # ND_FOR start\n");
        int begin_label = next_label_key();
        int end_label = next_label_key();
        // 初期化式
        if (node->init) {
          gen(node->init);
        }
        printf(".L.begin.%04d:\n", begin_label);
        // 条件式
        if (node->cond) {
          gen(node->cond);
          printf("  pop rax\n"); // 条件式の結果をraxにロード
          printf("  cmp rax, 0\n"); // 条件式の結果チェック
          printf("  je .L.end.%04d\n", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
        }
        // for の中身のアセンブラ
        gen(node->body);
        // 継続式
        if (node->inc) {
          gen(node->inc);
        }
        printf("  jmp .L.begin.%04d\n", begin_label); //繰り返し
        printf(".L.end.%04d:\n", end_label);

        printf("  # ND_FOR end\n");
      }
      return;
    case ND_CALL:
      {
        // 引数を渡すときにつかうレジスタは
        // https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf
        // の
        // Figure 3.4: Register Usage
        // を参照(引数1から引数6までは rdi, rsi, rdx, rcx, r8, r9の順に積む)
        printf("  # ND_CALL start\n");
        if (node->funcarg_num > 0) {
          Node *cur = node->arg;
          for (int i = 0; i < node->funcarg_num; i++) {
            // 引数のアセンブリを出力(どんどん引数の式の値がスタックに積まれる)
            printf("  # func call argument %d\n", i + 1);
            gen(cur);
            cur = cur->next;
          }

          // スタックから引数用のレジスタに値をロード
          for (int i = 0; i < node->funcarg_num; i++) {
            printf("  # load argument %d to register\n", i + 1);
            printf("  pop %s\n", ARGUMENT_REGISTERS[i]);
          }
        }
        printf("  call %s\n", node->funcname);
        printf("  push rax\n"); // 関数の戻り値をスタックに積む
        printf("  # ND_CALL end\n");
      }
      return;
    case ND_ADDR:
      printf("  # ND_ADDR start\n");
      gen_addr(node->lhs);
      printf("  # ND_ADDR end\n");
      return;
    case ND_DEREF:
      printf("  # ND_DEREF start\n");
      gen(node->lhs);
      printf("  pop rax\n"); // gen_addrでスタックに積んだアドレスを取得
      printf("  mov rax, [rax]\n"); // raxの値をアドレスとみなして、そのアドレスのにある値をraxにいれる
      printf("  push rax\n"); // 値をスタックにプッシュ
      printf("  # ND_DEREF end\n");
      return;
  }

  gen(node->lhs);
  gen(node->rhs);

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

void codegen(Function *func) {
  printf(".global %s\n", func->name);
  printf("%s:\n", func->name);
  // プロローグ
  // rbp初期化とローカル変数確保
  printf("  push rbp\n"); // 前の関数呼び出しでのrbpをスタックに対比
  printf("  mov rbp, rsp\n"); // この関数呼び出しでのベースポインタ設定
  // 使用されているローカル変数の数分、領域確保(ここに引数の値を保存する領域も確保される)
  printf("  sub rsp, %d\n", func->stack_size);

  // レジスタから引数の情報をスタックに確保
  int i = 0;
  for (VarList *v = func->params; v; v = v->next) {
    printf("  mov [rbp-%d], %s\n", v->var->offset, ARGUMENT_REGISTERS[i]);
    i++;
  }

  // fprintf(stderr, "func: %s, stack_size: %d\n", node->name, node->stack_size);
  // 先頭の文からコード生成
  for (Node *n = func->body; n; n = n->next) {
    gen(n);
    // 最後に演算結果がスタックの先頭にあるので、スタックが溢れないようにそれを1文毎にraxに対比
    printf("  pop rax\n");
  }

  // エピローグ
  // rbpの復元と戻り値設定
  // 最後の演算結果が、rax(forの最後でpopしてるやつ)にロードされてるのでそれをmainの戻り値として返す
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  printf("  ret\n");
}
