#ifndef INCLUDED_9CC
#define INCLUDED_9CC

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// tokenize.c
typedef enum {
  TK_RESERVED, // RESERVEDとなっているが今の時点では + または - の記号
  TK_IDENT, // 識別子
  TK_NUM, // 整数トークン
  TK_EOF, // 入力終了
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind; // トークンの型
  Token *next;    // 次の入力トークン
  int val;        // kindがTK_NUMの場合、その数値
  char *str;      // トークン文字列
  int len;        // str の長さ
};

Token * tokenize(char *p);
void free_tokens(Token *cur);
void dump_token(Token *token);

// 現在着目しているトークン
extern Token *token;
// 入力プログラム
extern char *user_input;

// parser.c

// ASTのノード種別
typedef enum {
  ND_ADD,     // +
  ND_SUB,     // -
  ND_MUL,     // *
  ND_DIV,     // /
  ND_LT,      // <
  ND_LTE,     // <=
  ND_EQL,     // ==
  ND_NOT_EQL, // !=
  ND_ASSIGN,  // =
  ND_LVAR,    // ローカル変数
  ND_NUM,     // 整数
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;
  int val;    // kindがND_NUMの場合に使う
  int offset; // kindがND_LVARの場合に使う(その変数のrbpからのオフセット)
};

void error_at(char *loc, char *fmt, ...);
void error(char *fmt, ...);
void free_nodes(Node *node);

extern Node *code[100];
void program();


// codegen.c
void codegen(Node *node);

#endif
