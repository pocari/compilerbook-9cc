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
    codegen(f);
    i++;
  }

  free_tokens(head);
  for (int i = 0; functions[i]; i++) {
    free_nodes(functions[i]);
  }
  free_lvars(locals);

  return 0;
}

