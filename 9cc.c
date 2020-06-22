#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>

typedef enum {
  TK_RESERVED, // RESERVEDとなっているが今の時点では + または - の記号
  TK_NUM, // 整数トークン
  TK_EOF, // 入力終了
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind; // トークンの型
  Token *next;    // 次の入力トークン
  int val;        // kindがTK_NUMの場合、その数値
  char *str;      // トークン文字列
};

// 現在着目しているトークン
Token *token;

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");

  exit(1);
}

bool consume(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op) {
    return false;
  }
  token = token->next;
  return true;
}

void expect(char op) {
  if (token->kind != TK_RESERVED || token->str[0] != op) {
    error("'%c'ではありません", op);
  }
  token = token->next;
}

int expect_number() {
  if (token->kind != TK_NUM) {
    error("数ではありません");
  }
  int val = token->val;
  token = token->next;

  return val;
}

bool at_eof() {
  return token->kind == TK_EOF;
}

Token *new_token(TokenKind kind, Token *cur, char *str) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  cur->next = tok;
  return tok;
}

void free_tokens(Token *cur) {
  while (cur->next) {
    Token *tmp = cur->next;
    free(cur);
    if (!tmp) {
      return;
    }
    cur  = tmp;
  }
}

Token * tokenize(char *p) {
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (*p == '+' || *p == '-') {
      cur = new_token(TK_RESERVED, cur, p++);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p);
      cur->val = strtol(p, &p, 10);
      continue;
    }

    error("トークナイズできません");
  }
  new_token(TK_EOF, cur, p);

  return head.next;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }

  // head はfree用
  Token *head = token = tokenize(argv[1]);

  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");


  printf("  mov rax, %d\n", expect_number());
  while (!at_eof()) {
    // - の場合もあるので最初はconsumeでチェック
    if (consume('+')) {
      printf("  add rax, %d\n", expect_number());
      continue;
    }

    // + じゃなかったら - しか今はありえないので expect でチェック
    expect('-');
    printf("  sub rax, %d\n", expect_number());
  }
  printf("  ret\n");

  free_tokens(head);

  return 0;
}

