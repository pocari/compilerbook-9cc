#ifndef INCLUDED_YNICC
#define INCLUDED_YNICC

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct Node Node;
typedef struct Var Var;
typedef struct VarList VarList;
typedef struct Program Program;
typedef struct Function Function;
typedef struct Token Token;

// tokenize.c
typedef enum {
  TK_DUMMY,    // dummy
  TK_RESERVED, // RESERVEDとなっているが今の時点では + または - の記号
  TK_IDENT,    // 識別子
  TK_NUM,      // 整数トークン
  TK_RETURN,   // return
  TK_WHILE,    // while
  TK_IF,       // if
  TK_ELSE,     // else
  TK_FOR,      // for
  TK_INT,      // int型
  TK_CHAR,     // char型
  TK_SIZEOF,   // sizeof キーワード
  TK_STR,      // 文字列リテラル
  TK_EOF,      // 入力終了
} TokenKind;

struct Token {
  TokenKind kind; // トークンの型
  Token *next;    // 次の入力トークン
  int val;        // kindがTK_NUMの場合、その数値
  char *str;      // トークン文字列
  int len;        // str の長さ

  // 文字列リテラル
  char *contents;
  // 文字列リテラルの長さ
  int content_length;
};

Token * tokenize(char *p);
void free_tokens(Token *cur);
void dump_token(Token *token);
char *token_kind_to_s(TokenKind kind);

// 現在着目しているトークン
extern Token *token;
// 入力プログラム
extern char *user_input;

// type.c
typedef struct Type Type;
typedef enum {
  TY_DUMMY,
  TY_INT,
  TY_CHAR,
  TY_PTR,
  TY_ARRAY,
} TypeKind;

struct Type {
  TypeKind kind;
  int size; //この方のサイズ(TY_ARRAYの場合は sizeof(要素) * array_size, それ以外の場合はsizeof(要素))
  Type *ptr_to;
  size_t array_size; // kind が TY_ARRAY のときに配列サイズがセットされる
};

extern Type *int_type;
extern Type *char_type;
void add_type(Node *node);
bool is_integer(Type *t);
bool is_pointer(Type *t);
Type *pointer_to(Type *t);
int node_type_size(Node * node);
Type *new_type(TypeKind kind, Type *ptr_to, int size);
Type *array_of(Type *ptr_to, int array_size);



// parser.c

// ASTのノード種別
typedef enum {
  ND_DUMMY,     // dummy
  ND_ADD,       // num + num
  ND_PTR_ADD,   // num + ptr, ptr + num
  ND_SUB,       // num - num
  ND_PTR_SUB,   // ptr - num
  ND_PTR_DIFF,  // ptr - ptr
  ND_MUL,       // *
  ND_DIV,       // /
  ND_LT,        // <
  ND_LTE,       // <=
  ND_EQL,       // ==
  ND_NOT_EQL,   // !=
  ND_ASSIGN,    // =
  ND_VAR,      // ローカル変数
  ND_RETURN,    // return
  ND_WHILE,     // while
  ND_BLOCK,     // { stmt* } のブロック
  ND_IF,        // if文
  ND_FOR,       // for文
  ND_CALL,      // 関数呼び出し
  ND_ADDR,      // &演算子でのアドレス取得
  ND_DEREF,     // *演算子でのアドレス参照
  ND_VAR_DECL,  // 変数定義
  ND_EXPR_STMT, // 式文
  ND_NUM,       // 整数
} NodeKind;

struct Program {
  VarList *global_var;
  Function *functions;
};

struct Function {
  Function *next; //次の関数
  Type *return_type; //戻り値の型
  char *name; // 関数名
  Node *body; // 関数定義本体
  VarList *locals; //この関数のローカル変数情報
  VarList *params; //この関数の仮引数
  int stack_size; //この関数のスタックサイズ
};

struct Node {
  NodeKind kind;
  Type *ty;

  Node *lhs; // 二項演算での左辺
  Node *rhs; // 二項演算での右辺

  Node *body; //while, forとかの本文
  Node *next; // ブロックや関数引数など複数Nodeになる場合の次のnode

  Node *cond; // if, while, for の条件式
  Node *then; // ifの then節
  Node *els; // ifのelse節

  Node *init; // forの初期化式
  Node *inc; // forの継続式

  Node *arg; //関数の引数

  Var *var; //ND_VAR, ND_VAR_DECLのときの変数情報
  int val;    // kindがND_NUMの場合に使う
  char *funcname; // 関数名
  int funcarg_num; // 関数呼び出しの引数の数
};

struct Var {
  Type *type; // この変数の型
  char *name; // この変数の名前
  int offset; // rbpからのオフセット
  bool is_local; // local、global変数の識別用フラグ

  //文字列リテラル用の変数
  char *contents;
  int content_length;
};

struct VarList {
  VarList *next; // 次の変数
  Var *var; // 変数の実体へのポインタ
};

void error_at(char *loc, char *fmt, ...);
void error(char *fmt, ...);
void free_program(Program *function);
char *node_kind_to_s(Node *nd);
char *my_strndup(char *str, int len);
Node *new_node(NodeKind kind, Node *lhs, Node *rhs);
Program *program();

// codegen.c
void codegen(Program *prg);

// debug.c

char *function_body_ast(Function *f);
char *node_ast(Node *node);
void dump_function(Function *f);
void dump_globals(VarList *vars);
void dump_tokens(Token *t);

// string_buffer.c
typedef struct StringBuffer StringBuffer;

StringBuffer *sb_init();
void sb_append_char(StringBuffer *sb, char ch);
void sb_free(StringBuffer *sb);
char *sb_str(StringBuffer *sb);
int sb_str_len(StringBuffer *sb);

#endif
