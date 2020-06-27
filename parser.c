#include "9cc.h"
#include <stdlib.h>

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));

  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;

  return node;
}

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));

  node->kind = ND_NUM;
  node->val = val;

  return node;
}

void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");

  exit(1);
}

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");

  exit(1);
}

bool consume(char *op) {
  if (token->kind != TK_RESERVED ||
      token->len != strlen(op) ||
      memcmp(token->str, op, token->len)) {
    return false;
  }
  token = token->next;
  return true;
}

Token *consume_ident() {
  if (token->kind == TK_IDENT) {
    Token *ret = token;
    token = token->next;

    return ret;
  }
  return NULL;
}

void expect(char *op) {
  if (token->kind != TK_RESERVED ||
      token->len != strlen(op) ||
      memcmp(token->str, op, token->len)) {
    error("'%c'(token->kind: %d, token->len: %d, strlen(op): %d)ではありません", op, token->kind, token->len, strlen(op));
  }
  token = token->next;
}

int expect_number() {
  if (token->kind != TK_NUM) {
    error_at(token->str, "数ではありません。");
  }
  int val = token->val;
  token = token->next;

  return val;
}

bool at_eof() {
  return token->kind == TK_EOF;
}

LVar *dummy_lvar() {
  LVar *var = calloc(1, sizeof(LVar));
  var->next = NULL;
  var->offset = 0;

  return var;
}

LVar *find_lvar(Token *token) {
  for (LVar *var = locals; var; var = var->next) {
    if (var->len == token->len &&
        memcmp(var->name, token->str, var->len) == 0) {
      return var;
    }
  }
  return NULL;
}


// program    = stmt*
// stmt       = expr ";"
// expr       = assign
// assign     = equality (= assign)?
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add        = mul ("+" mul | "-" mul)*
// mul        = unary ("*" unary | "/" unary)*
// unary      = ("+" | "-")? primary
// primary    = num | ident | "(" expr ")"

void program();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

Node *code[100];
LVar *locals = NULL;

void program() {
  int i = 0;
  while (!at_eof()) {
    code[i++] = stmt();
  }
  code[i] = NULL;
}

Node *stmt() {
  Node *node = expr();
  expect(";");
  return node;
}

Node *expr() {
  return assign();
}

Node *assign() {
  Node *node = equality();

  if (consume("=")) {
    return new_node(ND_ASSIGN, node, assign());
  }

  return node;
}

Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_node(ND_EQL, node, relational());
    } else if (consume("!=")) {
      node = new_node(ND_NOT_EQL, node, relational());
    } else {
      return node;
    }
  }
}

Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<=")) {
      node = new_node(ND_LTE, node, add());
    } else if (consume(">=")) {
      node = new_node(ND_LTE, add(), node);
    } else if (consume("<")) {
      node = new_node(ND_LT, node, add());
    } else if (consume(">")) {
      node = new_node(ND_LT, add(), node);
    } else {
      return node;
    }
  }
}

Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+")) {
      node = new_node(ND_ADD, node, mul());
    } else if (consume("-")) {
      node = new_node(ND_SUB, node, mul());
    } else {
      return node;
    }
  }
}

Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_node(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = new_node(ND_DIV, node, unary());
    } else {
      return node;
    }
  }
}

Node *unary() {
  if (consume("+")) {
    return primary();
  } else if (consume("-")) {
    return new_node(ND_SUB, new_node_num(0), primary());
  }
  return primary();
}

Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *t = consume_ident();
  // fprintf(stderr, "try,consume_ident\n");
  if (t) {
    // fprintf(stderr, "true,consume_ident\n");
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;

    LVar *lvar = find_lvar(t);
    if (lvar) {
      // 既存のローカル変数の場合
      node->offset = lvar->offset;
    } else {
      // 初めて出てきたローカル変数の場合
      lvar = calloc(1, sizeof(LVar));
      lvar->name = t->str;
      lvar->len = t->len;
      lvar->next = locals;
      // 新しく変数確保するのでオフセットも広げる
      lvar->offset = locals->offset + 8;
      node->offset = lvar->offset;
      locals = lvar;
    }
    return node;
  }

  // () でも ident でもなければ 整数
  return new_node_num(expect_number());
}

void free_nodes(Node *node) {
  if (node->lhs) {
    free_nodes(node->lhs);
  }
  if (node->rhs) {
    free_nodes(node->rhs);
  }
  free(node);
}

void free_lvars(LVar *var) {
  while (var) {
    LVar *tmp = var->next;
    free(var);
    var = tmp;
  }
}

int count_lvar() {
  int i = 0;
  LVar *v = locals;
  while (v) {
    i++;
    v = v->next;
  }
  // 最後の1つはダミーなので1引いて返す
  return i - 1;
}

