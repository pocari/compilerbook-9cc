#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

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

  // head はfree用
  Token *head = token = tokenize(user_input);
  // fprintf(stderr, "-------------------------------- tokenized\n");

  // パースする(結果は グローバル変数のcode[100]に入る)
  program();

  // fprintf(stderr, "-------------------------------- parsed\n");
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");


  // プロローグ
  // rbp初期化とローカル変数確保
  // 識別子用のアルファベット小文字26(=8byte * 26 = 208byte)文字分の領域確保
  printf("  push rbp\n"); // 前の関数呼び出しでのrbpをスタックに対比
  printf("  mov rbp, rsp\n"); // この関数呼び出しでのベースポインタ設定
  printf("  sub rsp, 208\n"); // ローカル変数確保

  // 先頭の文からコード生成
  for (int i = 0; code[i]; i++) {
    codegen(code[i]);

    // 最後に演算結果がスタックの先頭にあるので、スタックが溢れないようにそれを1文毎にraxに対比
    printf("  pop rax\n");
  }

  // エピローグ
  // rbpの復元と戻り値設定
  // 最後の演算結果が、rax(forの最後でpopしてるやつ)にロードされてるのでそれをmainの戻り値として返す
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  printf("  ret\n");

  free_tokens(head);
  for (int i = 0; code[i]; i++) {
    free_nodes(code[i]);
  }

  return 0;
}

