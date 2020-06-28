#include "9cc.h"

// 現在着目しているトークン
Token *token;

// 入力プログラム
char *user_input;

void set_stack_info(Function *f) {
  int offset = 0;
  for (LVar *var = f->locals; var; var = var->next) {
    offset += 8;
    var->offset = offset;
  }
  f->stack_size = offset;
}


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

  // パースする(結果は グローバル変数のfunctionsに入る)
  program();

  // fprintf(stderr, "-------------------------------- parsed\n");
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");

  for (Function *f = functions; f; f = f->next) {
    set_stack_info(f);
    codegen(f);
  }

  free_tokens(head);
  free_lvars(locals);

  return 0;
}

