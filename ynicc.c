#include "ynicc.h"

// 現在着目しているトークン
Token *token;

// 入力プログラム
char *user_input;

int main(int argc, char **argv) {
  bool dump_ast = false;
  bool dump_ast_only = false;

  if (argc < 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  if (argc > 2) {
    for (int i = 1; i < argc - 1; i++) {
      if (strcmp(argv[i], "--ast") == 0) {
        dump_ast = true;
      }
      if (strcmp(argv[i], "--ast-only") == 0) {
        dump_ast = true;
        dump_ast_only = true;
      }
    }
  }

  // プログラム全体を保存
  user_input = argv[argc - 1];

  // head はfree用
  Token *head = token = tokenize(user_input);
  // fprintf(stderr, "-------------------------------- tokenized\n");

  // パースする(結果は グローバル変数のfunctionsに入る)
  Program *pgm = program();

  // fprintf(stderr, "-------------------------------- parsed\n");

  if (dump_ast) {
    dump_globals(pgm->global_var);
    for (Function *f = pgm->functions; f; f = f->next) {
      if (dump_ast) {
        dump_function(f);
      }
    }
  }

  if (!dump_ast_only) {
    codegen(pgm);
  }

  free_tokens(head);
  free_program(pgm);

  return 0;
}

