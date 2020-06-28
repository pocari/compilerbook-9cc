#include "9cc.h"

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

bool consume_kind(TokenKind kind) {
  if (token->kind != kind) {
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
    dump_token(token);
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

// strndup が mac側になくてlspでエラーになるので作る
char *my_strndup(char *str, int len) {
  char *buf = calloc(len + 1, sizeof(char));

  int i;
  for (i = 0; i < len; i++) {
    buf[i] = str[i];
  }
  buf[i] = '\0';

  return buf;
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


// program       = function_decl*
// function_def  = ident "(" params? ")" "{" stmt* "}"
// stmt          = expr ";"
//               | "{" stmt* "}"
//               | "return" expr ";"
//               | "if" "(" expr ")" stmt ("else" stmt)?
//               | "while" "(" expr ")" stmt
//               | "for" "(" expr? ";" expr? ";" expr? ")" stmt
// expr          = assign
// assign        = equality (= assign)?
// equality      = relational ("==" relational | "!=" relational)*
// relational    = add ("<" add | "<=" add | ">" add | ">=" add)*
// add           = mul ("+" mul | "-" mul)*
// mul           = unary ("*" unary | "/" unary)*
// unary         = ("+" | "-")? primary
// primary       = num | ident ("(" arg_list? ")")? | "(" expr ")"

void program();
Node *function_def();
void function_body();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

Node *functions[100];
LVar *locals = NULL;

void program() {
  int i = 0;
  while (!at_eof()) {
    functions[i++] = function_def();
  }

  functions[i] = NULL;
}

// function_def  = ident "(" params? ")" "{" funcgtion_body "}"
Node *function_def() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_FUNC_DEF;

  Token *t = consume_ident();
  if (!t) {
    error("関数定義がありません");
  }
  node->funcname = my_strndup(t->str, t->len);

  expect("(");
  // とりあえず引数なしの関数定義のみ
  expect(")");
  expect("{");

  // 関数本体
  int i = 0;
  while (!consume("}")) {
    node->code[i++] = stmt();
  }
  node->code[i] = NULL;

  return node;
}

Node *stmt() {
  Node *node;

  if (consume_kind(TK_RETURN)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr();
    expect(";");
  } else if (consume_kind(TK_IF)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;

    expect("(");
    node->code[0] = expr();
    expect(")");
    node->code[1] = stmt();

    // elseがあればパース
    if (consume_kind(TK_ELSE)) {
      node->code[2] = stmt();
    } else {
      node->code[2] = NULL;
    }
  } else if (consume_kind(TK_WHILE)) {
    expect("(");
    Node *condition = expr();
    expect(")");
    Node *body = stmt();
    node = new_node(ND_WHILE, condition, body);
  } else if (consume_kind(TK_FOR)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    expect("(");
    if (consume(";")) {
      //次が";"なら初期化式なしなのでNULL入れる
      node->code[0] = NULL;
    } else {
      // ";"でないなら初期化式があるのでパース
      node->code[0] = expr();
      expect(";");
    }
    if (consume(";")) {
      //次が";"なら条件式なしなのでNULL入れる
      node->code[1] = NULL;
    } else {
      // ";"でないなら条件式があるのでパース
      node->code[1] = expr();
      expect(";");
    }
    if (consume(")")) {
      //次が")"なら継続式なしなのでNULL入れる
      node->code[2] = NULL;
    } else {
      // ")"でないなら継続式があるのでパース
      node->code[2] = expr();
      expect(")");
    }
    node->code[3] = stmt();
  } else if (consume("{")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    int i = 0;
    while (!consume("}")) {
      node->code[i++] = stmt();
    }
    node->code[i] = NULL;
  } else {
    node = expr();
    expect(";");
  }

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

Node *parse_lvar(Token *t) {
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

Node *parse_call_func(Token *t) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_CALL;
  node->funcname = my_strndup(t->str, t->len);

  if (consume(")")) {
    // 閉じカッコがきたら引数なし関数
    return node;
  }

  // 引数パース
  int args = 0;
  node->code[args++] = expr();
  while (consume(",")) {
    node->code[args++] = expr();
  }
  node->funcarg_num = args;

  if (args > 6) {
    fprintf(stderr, "関数呼び出しの引数が6を超えると今はコンパイル出来ません\n");
  }
  expect(")");

  return node;
}

Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *t = consume_ident();
  if (t) {
    if (consume("(")) {
      // identに続けて"("があったら関数呼び出し
      return parse_call_func(t);
    } else {
      // "("がなかったらローカル変数
      return parse_lvar(t);
    }
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
  free(node->funcname);
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

