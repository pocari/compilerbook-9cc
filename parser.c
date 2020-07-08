#include "ynicc.h"

// 今パース中の関数のローカル変数
static VarList *locals = NULL;
Function *functions = NULL;

char *node_kind_to_s(Node *nd) {
  switch (nd->kind) {
    case  ND_DUMMY:
      return "ND_DUMMY";
    case  ND_ADD:
      return  "ND_ADD";
    case  ND_PTR_ADD:
      return  "ND_PTR_ADD";
    case  ND_SUB:
      return  "ND_SUB";
    case  ND_PTR_SUB:
      return  "ND_PTR_SUB";
    case  ND_PTR_DIFF:
      return  "ND_PTR_DIFF";
    case  ND_MUL:
      return  "ND_MUL";
    case  ND_DIV:
      return  "ND_DIV";
    case  ND_LT:
      return  "ND_LT";
    case  ND_LTE:
      return  "ND_LTE";
    case  ND_EQL:
      return  "ND_EQL";
    case  ND_NOT_EQL:
      return  "ND_NOT_EQ";
    case  ND_ASSIGN:
      return  "ND_ASSIGN";
    case  ND_LVAR:
      return  "ND_LVAR";
    case  ND_RETURN:
      return  "ND_RETURN";
    case  ND_WHILE:
      return  "ND_WHILE";
    case  ND_BLOCK:
      return  "ND_BLOCK";
    case  ND_IF:
      return  "ND_IF";
    case  ND_FOR:
      return  "ND_FOR";
    case  ND_CALL:
      return  "ND_CALL";
    case  ND_ADDR:
      return  "ND_ADDR";
    case  ND_DEREF:
      return  "ND_DEREF";
    case  ND_VAR_DECL:
      return  "ND_VAR_DECL";
    case  ND_EXPR_STMT:
      return  "ND_EXPR_STMT";
    case  ND_NUM:
      return "ND_NUM  ";
  };
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));

  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;

  add_type(node);
  return node;
}

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));

  node->kind = ND_NUM;
  node->val = val;

  return node;
}

// ローカル変数の確保＋このfunctionでのローカル変数のリストにも追加
LVar *new_lvar(char *name, Type *type) {
  LVar *lvar = calloc(1, sizeof(LVar));
  lvar->type = type;
  lvar->name = name;

  VarList *v = calloc(1, sizeof(VarList));
  v->var = lvar;
  v->next = locals;

  // fprintf(stderr, "new_lvar:next ... %s\n", locals ? "nanika" : "NULL");
  locals = v;

  return lvar;
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
    error_at(token->str, "'%c'(token->kind: %s, token->len: %d, strlen(op): %d)ではありません", op, token_kind_to_s(token->kind), token->len, strlen(op));
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

char *expect_ident() {
  if (token->kind != TK_IDENT) {
    error_at(token->str, "識別子ではありません。");
  }
  char *s = my_strndup(token->str, token->len);
  token = token->next;
  return s;
}

void expect_token(TokenKind kind) {
  if (token->kind != kind) {
    error_at(token->str, "トークンが %s ではありません。", token_kind_to_s(kind));
  }
  token = token->next;
}

bool at_eof() {
  return token->kind == TK_EOF;
}

LVar *find_lvar(Token *token) {
  for (VarList *var_list = locals; var_list; var_list = var_list->next) {
    // fprintf(stderr, "var_name: %s\n", var->name);
    if (strlen(var_list-> var->name) == token->len &&
        memcmp(var_list-> var->name, token->str, token->len) == 0) {
      return var_list->var;
    }
  }
  return NULL;
}

// ynicc BNF
//
// program       = function_def*
// function_def  = type_in_decl ident "(" function_params? ")" "{" stmt* "}"
// stmt          = expr ";"
//               | "{" stmt* "}"
//               | "return" expr ";"
//               | "if" "(" expr ")" stmt ("else" stmt)?
//               | "while" "(" expr ")" stmt
//               | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//               | var_decl
// var_decl      = type_in_decl ident ";"
// type_in_decl  = "int" ("*" *)
// expr          = assign
// assign        = equality (= assign)?
// equality      = relational ("==" relational | "!=" relational)*
// relational    = add ("<" add | "<=" add | ">" add | ">=" add)*
// add           = mul ("+" mul | "-" mul)*
// mul           = unary ("*" unary | "/" unary)*
// unary         = "+"? postfix
//               | "-"? postfix
//               | "&" unary
//               | "*" unary
//               | "sizeof" unary
// postfix       = parimary ("[" expr "]")*
// primary       = num
//               | ident ("(" arg_list? ")")?
//               | "(" expr ")"

void program();
Function *function_def();
Node *stmt();
Type *type_in_decl();
Node *var_decl();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *postfix();
Node *primary();
Node *read_expr_stmt();

void program() {
  Function head = {};
  Function *cur = &head;
  while (!at_eof()) {
    cur->next = function_def();
    cur = cur->next;
  }
  functions = head.next;
}

// type_in_decl  = "int" ("*" *)
Type *type_in_decl() {
  Type *t;
  expect_token(TK_INT);
  t = int_type;
  while (consume("*")) {
    t = pointer_to(t);
  }
  return t;
}

void function_params(Function *func) {
  if (consume(")")) {
    return;
  }

  Type *type = type_in_decl();
  LVar *var = new_lvar(expect_ident(), type);
  VarList *var_list = calloc(1, sizeof(VarList));
  var_list->var = var;
  // fprintf(stderr, "parse func param start\n");
  while (!consume(")")) {
    expect(",");
    type = type_in_decl();
    LVar *var = new_lvar(expect_ident(), type);
    VarList *v = calloc(1, sizeof(VarList));
    v->var = var;
    v->next = var_list;
    var_list = v;
  }
  // fprintf(stderr, "parse func param end\n");

  func->params = var_list;
}

// 関数のスタックサイズ関連を計算
void set_stack_info(Function *f) {
  int offset = 0;
  for (VarList *v = f->locals; v; v = v->next) {
    offset += v->var->type->size;
    v->var->offset = offset;
  }
  f->stack_size = offset;
}

Function *function_def() {
  // 今からパースする関数ようにグローバルのlocalsを初期化
  locals = NULL;

  Type *ret_type = type_in_decl();
  Token *t = consume_ident();
  if (!t) {
    error("関数定義がありません");
  }
  Function *func = calloc(1, sizeof(Function));
  func->return_type = ret_type;
  func->name = my_strndup(t->str, t->len);

  expect("(");
  function_params(func);
  expect("{");

  // 関数本体
  int i = 0;
  Node head = {};
  Node *cur = &head;
  // fprintf(stderr, "parse function body start\n");
  while (!consume("}")) {
    // dump_token(token);
    cur->next = stmt();
    cur = cur->next;
  }
  // fprintf(stderr, "parse function body end\n");
  func->body = head.next;
  func->locals = locals;
  set_stack_info(func);

  return func;
}

Node *read_expr_stmt() {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_EXPR_STMT;
  node->lhs = expr();
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
    node->cond = expr();
    expect(")");
    node->then = stmt();

    // elseがあればパース
    if (consume_kind(TK_ELSE)) {
      node->els = stmt();
    } else {
      node->els = NULL;
    }
  } else if (consume_kind(TK_WHILE)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_WHILE;

    expect("(");
    node->cond = expr();
    expect(")");
    node->body = stmt();
  } else if (consume_kind(TK_FOR)) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    expect("(");
    if (consume(";")) {
      //次が";"なら初期化式なしなのでNULL入れる
      node->init = NULL;
    } else {
      // ";"でないなら初期化式があるのでパース
      node->init = read_expr_stmt();
      expect(";");
    }
    if (consume(";")) {
      //次が";"なら条件式なしなのでNULL入れる
      node->cond = NULL;
    } else {
      // ";"でないなら条件式があるのでパース
      node->cond = expr();
      expect(";");
    }
    if (consume(")")) {
      //次が")"なら継続式なしなのでNULL入れる
      node->inc = NULL;
    } else {
      // ")"でないなら継続式があるのでパース
      node->inc = read_expr_stmt();
      expect(")");
    }
    node->body = stmt();
  } else if (consume("{")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    int i = 0;
    Node head = {};
    Node *cur = &head;
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    node->body = head.next;
  } else {
    // キーワードじゃなかったら 変数宣言かどうかチェック
    node = var_decl();
    if (!node) {
      // 変数宣言でなかったら式文
      node = read_expr_stmt();
      expect(";");
    }
  }

  add_type(node);
  return node;
}

// int *x[n]
// のxの直後の[n]をパースする
Type *read_type_suffix(Type *base) {
  if (!consume("[")) {
    return base;
  }
  int array_size = expect_number();
  expect("]");
  return array_of(base, array_size);
}

Node *var_decl() {
  if (token->kind == TK_INT) {
    Type *type = type_in_decl();
    char *ident_name = expect_ident();
    // 識別子につづく配列用の宣言をパースして型情報を返す
    type = read_type_suffix(type);
    LVar *var = new_lvar(ident_name, type);
    expect(";");
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_VAR_DECL;
    node->var = var;
    return node;
  }
  return NULL;
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

Node *new_add_node(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  // int x
  // &x + 2
  // => & に intのサイズ(4) * 2 => xのアドレスの 8byte先
  //
  // int x
  // int *y = &x
  // int **z = &y
  // &z + 2
  // => & に intのポインタのサイズ(8) * 2 => zのアドレスの 16byte先
  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    // num + num
    return new_node(ND_ADD, lhs, rhs);
  } else if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
    // ptr + num のケース
    return new_node(ND_PTR_ADD, lhs, rhs);
  } else if (is_integer(lhs->ty) && is_pointer(rhs->ty)){
    // num + ptrのケースなので、入れ替えてptr + num に正規化する
    return new_node(ND_PTR_ADD, rhs, lhs);
  }
  // ここに来た場合 ptr + ptr という不正なパターンになるので落とす
  error_at(token->str, "不正なaddパターンです。");
  return NULL;
}

Node *new_sub_node(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    // num - num
    return new_node(ND_SUB, lhs, rhs);
  } else if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
    //  ptr - ptr
    return new_node(ND_PTR_DIFF, lhs, rhs);
  } else if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
    // ptr - num
    return new_node(ND_PTR_SUB, lhs, rhs);
  }
  // ここに来た場合 num - ptr になってしまうので、エラーにする
  error("不正な演算です: num - ptr");
  return NULL; // error で落ちるので実際にはreturnされない
}

Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+")) {
      node = new_add_node(node, mul());
    } else if (consume("-")) {
      node = new_sub_node(node, mul());
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
    return postfix();
  } else if (consume("-")) {
    return new_node(ND_SUB, new_node_num(0), postfix());
  } else if (consume("&")) {
    return new_node(ND_ADDR, unary(), NULL);
  } else if (consume("*")) {
    return new_node(ND_DEREF, unary(), NULL);
  } else if (consume_kind(TK_SIZEOF)) {
    Node *n = unary();
    add_type(n);
    return new_node_num(node_type_size(n));
  }
  return postfix();
}


Node *parse_lvar(Token *t) {
    LVar *lvar = find_lvar(t);
    if (!lvar) {
      error_at(t->str, "変数 %s は宣言されていません。", my_strndup(t->str, t->len));
    }
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;
    node->var = lvar;
    return node;
}

Node *postfix() {
  Node *node = primary();
  while (consume("[")) {
    node = new_node(ND_DEREF, new_add_node(node, expr()), NULL);
    expect("]");
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
  Node *cur = node->arg = expr();
  args++;
  while (consume(",")) {
    cur->next = expr();
    cur = cur->next;
    args++;
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
  if (!node) {
    return;
  }
  // LVar の free は別途VarListのfreeで一緒にする(共有しているため)
  free_nodes(node->next);
  free_nodes(node->arg);
  free_nodes(node->lhs);
  free_nodes(node->rhs);
  free_nodes(node->body);
  free_nodes(node->cond);
  free_nodes(node->then);
  free_nodes(node->els);
  free_nodes(node->init);
  free_nodes(node->inc);
  free(node->funcname);
  free(node);
}

void free_lvar(LVar *var) {
  if (var) {
    Type *t = var->type;
    while (t) {
      Type *tmp = t->ptr_to;
      if (!is_integer(t)) {
        free(t);
      }
      t = tmp;
    }
    free(var->name);
    var->name = NULL;
  }
  free(var);
}

void free_var_list_deep(VarList *var) {
  VarList *v = var;
  while (v) {
    VarList *tmp = v->next;
    free_lvar(v->var);
    v->var = NULL;
    free(v);
    v = tmp;
  }
}

void free_var_list_shallow(VarList *var) {
  VarList *v = var;
  while (v) {
    VarList *tmp = v->next;
    free(v);
    v = tmp;
  }
}

void free_function(Function *f) {
  free(f->name);
  // var_listの中のLVarは locals にローカル変数も関数の引数も含めて全部もっているので localsのfreeで一緒に削除する
  free_var_list_deep(f->locals);
  // 引数のvar_listの LVar * はlocalsのfreeでおこなｗれているので、こちらは VarList の freeのみ行う
  free_var_list_shallow(f->params);

  free_nodes(f->body);
  free(f);
}

void free_functions(Function *func) {
  Function *f = func;
  while (f) {
    Function *tmp = f->next;
    free_function(f);
    f = tmp;
  }
}

int type_info_helper(char *buf, int offset, Type *type) {
  int n = 0;
  switch (type->kind) {
    case TY_INT:
      n += sprintf(buf + offset, "int");
      break;
    case TY_PTR:
      n += sprintf(buf + offset, "*");
      n += type_info_helper(buf, offset + n, type->ptr_to);
      break;
    case TY_ARRAY:
      n += sprintf(buf + offset, "[%ld]", type->array_size);
      n += type_info_helper(buf, offset + n, type->ptr_to);
      break;
  }
  return n;
}

char *type_info(Type *type) {
  char buf[10000];
  int n = type_info_helper(buf, 0, type);
  return my_strndup(buf, n);
}

char *node_ast(Node *node) {
  char buf[10000];
  int n = 0;

  if (!node) {
    return NULL;
  }

  switch (node->kind) {
      case ND_DUMMY:
        break;
      case ND_ADD:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(+ %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_PTR_ADD:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(ptr+ %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_SUB:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(- %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_PTR_SUB:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(ptr- %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_PTR_DIFF:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(ptr-diff %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_MUL:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(* %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_DIV:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(/ %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_LT:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(< %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_LTE:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(<= %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_EQL:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(== %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_NOT_EQL:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(!= %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_ASSIGN:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(let %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_LVAR:
        n = sprintf(buf, "(lvar %s)", node->var->name);
        return my_strndup(buf, n);
      case ND_RETURN:
        {
          char *l = node_ast(node->lhs);
          n = sprintf(buf, "(return %s)", l);
          free(l);
          return my_strndup(buf, n);
        }
      case ND_WHILE:
        {
          char *cond = node_ast(node->cond);
          char *body = node_ast(node->body);
          n = sprintf(buf, "(while (cond %s) (body %s))", cond, body);
          char *ret = my_strndup(buf, n);
          free(cond);
          free(body);
          return ret;
        }
      case ND_BLOCK:
        {
          int i = 0;
          char *tmp_bufs[1000];
          n += sprintf(buf, "(block ");
          for (Node *nd = node->body; nd; nd = nd->next) {
            tmp_bufs[i] = node_ast(nd);
            n += sprintf(buf + n, "%s", tmp_bufs[i]);
            if (nd->next) {
              n += sprintf(buf + n, " ");
            }
            i++;
          }
          for (int j = 0; j < i; j++) {
            free(tmp_bufs[j]);
          }
          n += sprintf(buf + n, ")");
          return my_strndup(buf, n);
        }
      case ND_IF:
        {
          char *cond = node_ast(node->cond);
          char *then = node_ast(node->then);
          char *els = node_ast(node->els);
          n = sprintf(buf, "(if (cond %s) (then %s) (else %s))", node_ast(node->cond), node_ast(node->then), node_ast(node->els));
          char *ret = my_strndup(buf, n);
          free(cond);
          free(then);
          free(els);
          return ret;
        }
      case ND_FOR:
        {
          char *init = node_ast(node->init);
          char *cond = node_ast(node->cond);
          char *inc = node_ast(node->inc);
          char *body = node_ast(node->body);
          n = sprintf(buf, "(for (init %s) (cond %s) (inc %s) (body %s))", node_ast(node->init), node_ast(node->cond), node_ast(node->inc), node_ast(node->body));
          char *ret = my_strndup(buf, n);
          free(init);
          free(cond);
          free(inc);
          free(body);
          return ret;
        }
      case ND_CALL:
        {
          char *tmp_bufs[1000];
          n = sprintf(buf, "(call (name %s)", node->funcname);
          int i = 0;
          for (Node *p = node->arg; p; p = p->next) {
            char *tmp = tmp_bufs[i] = node_ast(p);
            n += sprintf(buf + n, " %s", tmp);
            i++;
          }
          n += sprintf(buf + n, ")");
          char *ret = my_strndup(buf, n);

          for (int j = 0; j < i; j++) {
            free(tmp_bufs[j]);
          }
          return ret;
        }
      case ND_ADDR:
        {
          char *l = node_ast(node->lhs);
          n = sprintf(buf, "(addr %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_DEREF:
        {
          char *l = node_ast(node->lhs);
          n = sprintf(buf, "(deref %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_VAR_DECL:
        {
          char *ti = type_info(node->var->type);
          n = sprintf(buf, "(decl (%s %s))", ti, node->var->name);
          char *ret = my_strndup(buf, n);
          free(ti);
          return ret;
        }
      case ND_EXPR_STMT:
        {
          char *l = node_ast(node->lhs);
          n = sprintf(buf, "(expr-stmt %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_NUM:
        n = sprintf(buf, "(num %d)", node->val);
        return my_strndup(buf, n);
  }
  return NULL;
}

char *node_ast_helper(Node *body) {
  char *tmp_bufs[1000];// 文の数
  char buf[10000];// ast dumpの文字列バッファ

  int n = 0;
  int i = 0;

  n += sprintf(buf + n, "\n");
  for (Node *nd = body; nd; nd = nd->next) {
    char *tmp = tmp_bufs[i] = node_ast(nd);
    n += sprintf(buf + n, "##   %s\n", tmp);
  }
  char *ret = my_strndup(buf, n);
  for (int j = 0; j < i; j++) {
    free(tmp_bufs[j]);
  }
  return ret;
}

char *function_body_ast(Function *f) {
  int n = 0;
  char buf[10000];

  char *ti = type_info(f->return_type);
  n = sprintf(buf + n, "(def (%s) %s (", ti, f->name);
  free(ti);

  int i = 0;
  VarList* reverse_params = NULL;
  // f->paramsは最後の引数からのリンクリストになってるのでダンプ用には逆順にする。
  for (VarList *cur = f->params; cur; cur = cur->next) {
    VarList *vl = calloc(1, sizeof(VarList));
    vl->next = reverse_params;
    vl->var = cur->var;
    reverse_params = vl;
  }

  // 逆順にしたリストからastダンプ
  for (VarList *p = reverse_params; p; p = p->next) {
    char *ti = type_info(p->var->type);
    n += sprintf(buf + n, "%s %s", ti, p->var->name);
    free(ti);
    if (p->next) {
      n += sprintf(buf + n, " ");
    }
  }
  // 逆順のリストはもういらないので捨てる。(そこに紐づくLVar は別途freeされるので無視する)
  VarList *rp = reverse_params;
  while (rp) {
    VarList *tmp = rp->next;
    free(rp);
    rp = tmp;
  }

  char *body_ast = node_ast_helper(f->body);
  n += sprintf(buf + n, ") ( %s", body_ast);
  n += sprintf(buf + n, "## ))");
  free(body_ast);

  return my_strndup(buf, n);
}
