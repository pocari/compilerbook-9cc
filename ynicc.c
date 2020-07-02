#include "ynicc.h"

// 現在着目しているトークン
Token *token;

// 入力プログラム
char *user_input;

void dump_function(Function *f) {
  char *ast = function_body_ast(f);
  printf("## %s\n", ast);
  free(ast);
}

int main(int argc, char **argv) {
  bool dump_ast = false;

  if (argc < 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  if (argc > 2) {
    for (int i = 1; i < argc - 1; i++) {
      if (strcmp(argv[i], "--ast") == 0) {
        dump_ast = true;
      }
    }
  }

  // プログラム全体を保存
  user_input = argv[argc - 1];

  // head はfree用
  Token *head = token = tokenize(user_input);
  // fprintf(stderr, "-------------------------------- tokenized\n");

  // パースする(結果は グローバル変数のfunctionsに入る)
  program();

  // fprintf(stderr, "-------------------------------- parsed\n");
  printf(".intel_syntax noprefix\n");

  for (Function *f = functions; f; f = f->next) {
    if (dump_ast) {
      dump_function(f);
    }
    codegen(f);
  }

  free_tokens(head);
  free_functions(functions);

  return 0;
}

