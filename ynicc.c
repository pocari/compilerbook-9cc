#include "ynicc.h"

// 現在着目しているトークン
Token *token;

// 入力ファイル名
char *filename;
// 入力プログラム
char *user_input;

int main(int argc, char **argv) {
  bool f_dump_ast = false;
  bool f_dump_ast_only = false;
  bool f_dump_tokens = false;

  if (argc < 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  if (argc > 2) {
    for (int i = 1; i < argc - 1; i++) {
      if (strcmp(argv[i], "--ast") == 0) {
        f_dump_ast = true;
      }
      if (strcmp(argv[i], "--ast-only") == 0) {
        f_dump_ast = true;
        f_dump_ast_only = true;
      }
      if (strcmp(argv[i], "--tokens") == 0) {
        f_dump_tokens = true;
      }
    }
  }

  // プログラム全体を保存
  filename = argv[argc - 1];
  user_input = read_file(filename);

  // head はfree用
  Token *head = token = tokenize(user_input);
  // fprintf(stderr, "-------------------------------- tokenized\n");
  if (f_dump_tokens) {
    printf("##-----------------------------\n");
    printf("## tokens\n");
    dump_tokens(head);
  }

  // パースする(結果は グローバル変数のfunctionsに入る)
  Program *pgm = program();


  if (f_dump_ast) {
    printf("##-----------------------------\n");
    printf("## ast\n");
    dump_globals(pgm->global_var);
    for (Function *f = pgm->functions; f; f = f->next) {
      if (f_dump_ast) {
        dump_function(f);
      }
    }
  }

  if (!f_dump_ast_only) {
    codegen(pgm);
  }

  free_tokens(head);

  return 0;
}

