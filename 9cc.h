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
  TK_IDENT,    // 識別子
  TK_NUM,      // 整数トークン
  TK_RETURN,   // return
  TK_WHILE,    // while
  TK_IF,       // if
  TK_ELSE,     // else
  TK_FOR,      // for
  TK_EOF,      // 入力終了
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
  ND_RETURN,  // return
  ND_WHILE,   // while
  ND_BLOCK,   // { stmt* } のブロック
  ND_IF,      // if文
  ND_FOR,     // for文
  ND_CALL,    // 関数呼び出し
  ND_NUM,     // 整数
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind;
  Node *lhs;
  Node *rhs;
  Node *code[20]; // blockの場合の中身と、ifの場合の0:cnd, 1: true_block, 2: else_block（とりあえず最大20 stmt)
  int val;    // kindがND_NUMの場合に使う
  int offset; // kindがND_LVARの場合に使う(その変数のrbpからのオフセット)
  char *funcname;
};

typedef struct LVar LVar;

struct LVar {
  LVar *next; // 次の変数
  char *name; // この変数の名前
  int len;    // 変数名の長さ
  int offset; // rbpからのオフセット
};

void error_at(char *loc, char *fmt, ...);
void error(char *fmt, ...);
void free_nodes(Node *node);
void free_lvars(LVar *var);
void program();
LVar *dummy_lvar();
int count_lvar();

// 文(stmmt)達
extern Node *code[100];
// ローカル変数達
extern LVar *locals;


// codegen.c
void codegen(Node *node);

#endif
