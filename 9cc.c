#include "9cc.h"

// 現在着目しているトークン
Token *token;

// 入力プログラム
char *user_input;

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // プログラム全体を保存
  user_input = argv[1];

  // ローカル変数の初期化
  locals = dummy_lvar();

  // head はfree用
  Token *head = token = tokenize(user_input);
  // fprintf(stderr, "-------------------------------- tokenized\n");

  // パースする(結果は グローバル変数のcode[100]に入る)
  program();

  // fprintf(stderr, "-------------------------------- parsed\n");
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");

  int i = 0;
  while (functions[i]) {
    Node *f = functions[i];
    printf("%s:\n", f->funcname);


    // プロローグ
    // rbp初期化とローカル変数確保
    printf("  push rbp\n"); // 前の関数呼び出しでのrbpをスタックに対比
    printf("  mov rbp, rsp\n"); // この関数呼び出しでのベースポインタ設定
    // 使用されているローカル変数の数分、領域確保
    printf("  sub rsp, %d\n", 8 * count_lvar());

    // 先頭の文からコード生成
    for (int i = 0; f->code[i]; i++) {
      codegen(f->code[i]);

      // 最後に演算結果がスタックの先頭にあるので、スタックが溢れないようにそれを1文毎にraxに対比
      printf("  pop rax\n");
    }

    // エピローグ
    // rbpの復元と戻り値設定
    // 最後の演算結果が、rax(forの最後でpopしてるやつ)にロードされてるのでそれをmainの戻り値として返す
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");

    i++;
  }

  free_tokens(head);
  for (int i = 0; functions[i]; i++) {
    free_nodes(functions[i]);
  }
  free_lvars(locals);

  return 0;
}

