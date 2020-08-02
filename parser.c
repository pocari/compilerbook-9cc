#include "ynicc.h"

// 今パース中の関数のローカル変数
static VarList *locals = NULL;

// グローバル変数
static VarList *globals = NULL;

static Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

static Node *new_bin_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;

  add_type(node);

  return node;
}

static Node *new_unary_node(NodeKind kind, Node *lhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  return node;
}

static Node *new_num_node(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;

  return node;
}

static Var *new_var(char *name, Type *type, bool is_local) {
  Var *var = calloc(1, sizeof(Var));
  var->type = type;
  var->name = name;
  var->is_local = is_local;

  return var;
}

// ローカル変数の確保＋このfunctionでのローカル変数のリストにも追加
static Var *new_lvar(char *name, Type *type) {
  Var *var = new_var(name, type, true);

  VarList *v = calloc(1, sizeof(VarList));
  v->var = var;
  v->next = locals;

  // fprintf(stderr, "new_lvar:next ... %s\n", locals ? "nanika" : "NULL");
  locals = v;

  return var;
}

static Var *new_gvar(char *name, Type *type) {
  Var *var = new_var(name, type, false);

  VarList *v = calloc(1, sizeof(VarList));
  v->var = var;
  v->next = globals;
  globals = v;

  return var;
}

static int error_at_line_num(char *loc) {
  // user_input から loc までの間に何個改行があったか計算
  char *p = user_input;
  int num_lines = 0;

  while (p != loc) {
    if (*p == '\n') {
      num_lines++;
    }
    p++;
  }

  return num_lines;
}

static int get_pos_by_line(char *loc) {
  // locがその行で何桁目か返す
  char *p = user_input;
  int column = 0;

  while (p != loc) {
    if (*p == '\n') {
      column = 0;
    } else {
      column++;
    }
    p++;
  }

  return column;
}

static char *get_error_lines(char *loc) {
  //エラーが出た行までの文字列を返す
  int pos = 0;
  char *p = user_input;

  while (p != loc) {
    pos++;
    p++;
  }

  // locまできたら、その行の行末または文字列の最後まですすめる
  while (*p && *p != '\n') {
    pos++;
    p++;
  }

  return my_strndup(user_input, pos);
}

void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int column = get_pos_by_line(loc);
  fprintf(stderr, "file: %s\n", filename);
  fprintf(stderr, "%s\n", get_error_lines(loc));
  fprintf(stderr, "%*s", column, "");
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

static int align_to(int n, int align) {
  // chibicc では、
  //   return (n + align - 1) & ~(align - 1);
  // という速そうなコードだった。
  // https://github.com/rui314/chibicc/commit/b6d512ee69ea455031cf57612437e238f233a293#diff-2045016cb90d1e65d71c2407a2570927R4

  // 取り敢えず挙動としては、align = 8 の場合
  //  0     -> 0
  //  1.. 8 -> 8
  //  9..16 -> 16
  // 17..24 -> 24
  // ....
  // となれば良さそうなのでそうなるように計算する
  return ((n / align) + (n % align == 0 ? 0 : 1)) * align;
}

static bool peek_token(char *op) {
  if (token->kind != TK_RESERVED ||
      token->len != strlen(op) ||
      memcmp(token->str, op, token->len)) {
    return false;
  }
  return true;
}

static bool consume(char *op) {
  if (!peek_token(op)) {
    return false;
  }
  token = token->next;
  return true;
}

static Token *consume_kind(TokenKind kind) {
  if (token->kind != kind) {
    return NULL;
  }

  Token *t = token;
  token = token->next;

  return t;
}

static Token *consume_ident() {
  if (token->kind == TK_IDENT) {
    Token *ret = token;
    token = token->next;

    return ret;
  }
  return NULL;
}

static void expect(char *op) {
  if (token->kind != TK_RESERVED ||
      token->len != strlen(op) ||
      memcmp(token->str, op, token->len)) {
    error_at(token->str, "'%c'(token->kind: %s, token->len: %d, strlen(op): %d, op: %s)ではありません", op, token_kind_to_s(token->kind), token->len, strlen(op), op);
  }
  token = token->next;
}

static int expect_number() {
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

static char *expect_ident() {
  if (token->kind != TK_IDENT) {
    error_at(token->str, "識別子ではありません。");
  }
  char *s = my_strndup(token->str, token->len);
  token = token->next;
  return s;
}

static void expect_token(TokenKind kind) {
  if (token->kind != kind) {
    error_at(token->str, "トークンが %s ではありません。", token_kind_to_s(kind));
  }
  token = token->next;
}

static bool at_eof() {
  return token->kind == TK_EOF;
}

static Var *find_var_helper(Token *token, VarList *vars) {
  for (VarList *var_list = vars; var_list; var_list = var_list->next) {
    if (strlen(var_list-> var->name) == token->len &&
        memcmp(var_list-> var->name, token->str, token->len) == 0) {
      return var_list->var;
    }
  }
  return NULL;
}

static Var *find_var(Token *token) {
  Var *v = NULL;

  // local変数から探す
  v = find_var_helper(token, locals);
  if (v) {
    return v;
  }

  // なかったらglobal変数から探す
  v = find_var_helper(token, globals);

  return v;
}

static bool is_type_token(TokenKind kind) {
  switch (kind) {
    case TK_INT:
    case TK_CHAR:
    case TK_STRUCT:
      return true;
    default:
      return false;
  }
}


// ynicc BNF
//
// program                   = (
//                               func_decls
//                             | global_var_decls
//                           )*
// function_def              = type_in_decl ident "(" function_params? ")" "{" stmt* "}"
// global_var_decls          = type_in_decl ident "read_type_suffix" ";"
// stmt                      = expr ";"
//                           | "{" stmt* "}"
//                           | "return" expr ";"
//                           | "if" "(" expr ")" stmt ("else" stmt)?
//                           | "while" "(" expr ")" stmt
//                           | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//                           | var_decl
// var_decl                  = type_in_decl ident ( "=" local_var_initializer)? ";"
// local_var_initializer     = local_var_initializer_sub
// local_var_initializer_sub = "{" local_var_initializer_sub ("," local_var_initializer_sub)* "}"
//                           | expr
// type_in_decl              = ("int" | "char" | struct_decl) ("*" *)
// struct_decl               = "struct" "{" struct_member* "}"
// struct_member             = type_in_decl ident
// expr                      = assign
// assign                    = equality (= assign)?
// equality                  = relational ("==" relational | "!=" relational)*
// relational                = add ("<" add | "<=" add | ">" add | ">=" add)*
// add                       = mul ("+" mul | "-" mul)*
// mul                       = unary ("*" unary | "/" unary)*
// unary                     = "+"? postfix
//                           | "-"? postfix
//                           | "&" unary
//                           | "*" unary
//                           | "sizeof" unary
// postfix                   = parimary ("[" expr "]" | "." ident)*
// primary                   = num
//                           | ident ("(" arg_list? ")")?
//                           | "(" expr ")"
//                           | str

Program *program();

static Function *function_def();
static Node *stmt();
static Type *type_in_decl();
static Node *var_decl();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *unary();
static Node *postfix();
static Node *primary();
static Node *read_expr_stmt();
static Type *read_type_suffix(Type *base);
static Type *struct_decl();
static Node *new_add_node(Node *lhs, Node *rhs);

static bool is_function_def() {
  // 先読みした結果今の位置に戻る用に今のtokenを保存
  Token *tmp = token;

  type_in_decl();
  bool is_func = consume_ident() && consume("(");

  // 先読みした結果、関数定義かグローバル変数かわかったので、tokenを元に戻す
  token = tmp;

  return is_func;
}

// グローバル変数のパース
static void parse_gvar() {
  Type *type = type_in_decl();
  char *ident_name = expect_ident();
  // 識別子につづく配列用の宣言をパースして型情報を返す
  type = read_type_suffix(type);
  expect(";");
  new_gvar(ident_name, type);
}

Program *program() {
  Function head = {};
  Function *cur = &head;
  while (!at_eof()) {
    if (is_function_def()) {
      cur->next = function_def();
      cur = cur->next;
    } else {
      parse_gvar();
      // 関数じゃない場合はグローバル変数
    }
  }
  Program *program = calloc(1, sizeof(Program));
  program->global_var = globals;
  program->functions = head.next;

  return program;
}

// type_in_decl  = type_keyword ("*" *)
static Type *type_in_decl() {
  Type *t;
  if (!is_type_token(token->kind)) {
    error_at(token->str, "typename expected");
  }
  if (token->kind == TK_INT) {
      t = int_type;
      expect_token(token->kind);
  } else if (token->kind == TK_CHAR) {
      t = char_type;
      expect_token(token->kind);
  } else if (token->kind == TK_STRUCT) {
      t = struct_decl();
  }
  while (consume("*")) {
    t = pointer_to(t);
  }
  return t;
}

static Member *struct_member() {
  Type *type = type_in_decl();
  char *ident = expect_ident();
  type = read_type_suffix(type);
  expect(";");

  Member *m = calloc(1, sizeof(Member));
  m->name = ident;
  m->ty = type;

  return m;
}

static Type *struct_decl() {
  expect_token(TK_STRUCT);
  expect("{");

  Member head = {};
  Member *cur = &head;

  while (!consume("}")) {
    cur->next = struct_member();
    cur = cur->next;
  }

  Type *type = calloc(1, sizeof(Type));
  type->kind = TY_STRUCT;
  type->members = head.next;

  int offset = 0;
  for (Member *m = type->members; m; m = m->next) {
    m->offset = align_to(offset, m->ty->align);
    offset += m->ty->size;

    if (type->align < m->ty->align) {
      // 構造体自身のアラインメントは、構造体のメンバーの中の最大のアラインメントに合わせる
      // 構造体自体のサイズの割当(ループの外)で効いてくるらしい
      type->align = m->ty->align;
    }
  }
  // アラインメント(このコメントを書いているときのintはまだ8バイト)
  // int, charが混在しているstructを定義すると下記のようになる
  //
  // struct {
  //   char x;
  //   int y;
  // } s1;
  //
  // の場合、アラインメントが考慮されていない場合
  // char 1バイト、int 8バイトの合計9バイトの構造体になるが、
  // アラインメントを考慮する場合、8バイト境界に置かれることになるので、
  // xの直後からyが始まるのではなく、7バイト後からyの8バイトが始まり合計16バイトの構造体になる
  type->size = align_to(offset, type->align);

  return type;
}

static void function_params(Function *func) {
  if (consume(")")) {
    return;
  }

  Type *type = type_in_decl();
  Var *var = new_lvar(expect_ident(), type);
  VarList *var_list = calloc(1, sizeof(VarList));
  var_list->var = var;
  // fprintf(stderr, "parse func param start\n");
  while (!consume(")")) {
    expect(",");
    type = type_in_decl();
    Var *var = new_lvar(expect_ident(), type);
    VarList *v = calloc(1, sizeof(VarList));
    v->var = var;
    v->next = var_list;
    var_list = v;
  }
  // fprintf(stderr, "parse func param end\n");

  func->params = var_list;
}

// 関数のスタックサイズ関連を計算
static void set_stack_info(Function *f) {
  int offset = 0;
  for (VarList *v = f->locals; v; v = v->next) {
    offset += v->var->type->size;
    v->var->offset = offset;
  }
  f->stack_size = align_to(offset, 8);
}

static Function *function_def() {
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
    cur->next = stmt();
    cur = cur->next;
  }
  // fprintf(stderr, "parse function body end\n");
  func->body = head.next;
  func->locals = locals;
  set_stack_info(func);

  return func;
}

static Node *read_expr_stmt() {
  Node *node = new_node(ND_EXPR_STMT);
  node->kind = ND_EXPR_STMT;
  node->lhs = expr();
  return node;
}

static Node *stmt() {
  Node *node;

  if (consume_kind(TK_RETURN)) {
    node = new_node(ND_RETURN);
    node->lhs = expr();
    expect(";");
  } else if (consume_kind(TK_IF)) {
    node = new_node(ND_IF);

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
    node = new_node(ND_WHILE);

    expect("(");
    node->cond = expr();
    expect(")");
    node->body = stmt();
  } else if (consume_kind(TK_FOR)) {
    node = new_node(ND_FOR);
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
    node = new_node(ND_BLOCK);
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
static Type *read_type_suffix(Type *base) {
  if (!consume("[")) {
    return base;
  }
  int array_size = expect_number();
  expect("]");
  base = read_type_suffix(base);
  return array_of(base, array_size);
}

// https://github.com/rui314/chibicc/blob/e1b12f2c3d0e4389f327fcaa7484b5e439d4a716/parse.c#L679
// 配列の初期化式の書く要素の位置を表す構造体
// int x[2][3] = {{1, 2, 3}, {4, 5, 6}};
// みたいな式があった場合各次元のindexがリンクリストでつながっていて、
// 例えば、 x[1][2] の要素を指す Designator は下記のようになる
//
// 1次元目のindex ... 1
// 2次元目のindex ... 2
// 
// 2次元目         1次元目
// +---------+     +---------+
// | next ---|---->| next ---|---> NULL
// +---------+     +---------+
// | idx: 2  |     | idx: 1  |
// +---------+     +---------+
//
// 一番深い次元からみて浅い次元の方に向かってリンクリストが作られる。
// 正確には、浅い次元は深い次元のすべての要素のnextになる。
// 例えば、 x[1][2]とx[1][3] の関係は、下記のようになる
//
// 2次元目         1次元目
// +---------+     +---------+
// | next ---|--+->| next ---|---> NULL
// +---------+  |  +---------+
// | idx: 2  |  |  | idx: 1  |
// +---------+  |  +---------+
//              |
// 2次元目      |
// +---------+  |
// | next ---|--+
// +---------+
// | idx: 3  |
// +---------+
// 
// なお、初期化式が配列じゃない場合は Designator は NULL になる。
typedef struct Designator Designator;
struct Designator {
  Designator *next;
  int idx;
};

static Node *new_desg_node_sub(Var *var, Designator *desg) {
  if (!desg) {
    return new_var_node(var);
  }
  Node *prev = new_desg_node_sub(var, desg->next);
  Node *node = new_add_node(prev, new_num_node(desg->idx));
  return new_unary_node(ND_DEREF, node);
}

static Node *new_desg_node(Var *var, Designator *desg, Node *rhs) {
  Node *lhs = new_desg_node_sub(var, desg);
  Node *assign = new_bin_node(ND_ASSIGN, lhs, rhs);
  return new_unary_node(ND_EXPR_STMT, assign);
}

static Node *local_var_initializer_sub(Node *cur, Var *var, Type *ty, Designator *desg) {
  static int count = 0;
  // fprintf(stdout, "count: %d, type: %d\n", count++, ty->kind);
  if (ty->kind == TY_ARRAY) {
    expect("{");
    int i = 0;

    do {
      Designator desg2 = {desg, i++};
      cur = local_var_initializer_sub(cur, var, ty->ptr_to, &desg2);
    } while(!peek_token("}") && consume(","));

    expect("}");

    return cur;
  }

  Node *e = expr();
  // fprintf(stdout, "val: %d\n", e->val);
  cur->next = new_desg_node(var, desg, e);
  return cur->next;
}

static Node *local_var_initializer(Var *var) {
  Node head = {};
  local_var_initializer_sub(&head, var, var->type, NULL);

  Node *node = new_node(ND_BLOCK);
  node->body = head.next;

  return node;
}

static Node *var_decl() {
  if (is_type_token(token->kind)) {
    Type *type = type_in_decl();
    char *ident_name = expect_ident();
    // 識別子につづく配列用の宣言をパースして型情報を返す
    type = read_type_suffix(type);
    Var *var = new_lvar(ident_name, type);
    Node *node = new_node(ND_VAR_DECL);
    node->var = var;
    // 初期化式があるかどうかチェック
    if (consume("=")) {
      // 今作ったlocal変数に初期化式の値を代入するノードを設定
      node->initializer = local_var_initializer(var);
    }
    expect(";");

    return node;
  }
  return NULL;
}

static Node *expr() {
  return assign();
}

static Node *assign() {
  Node *node = equality();

  if (consume("=")) {
    return new_bin_node(ND_ASSIGN, node, assign());
  }

  return node;
}

static Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_bin_node(ND_EQL, node, relational());
    } else if (consume("!=")) {
      node = new_bin_node(ND_NOT_EQL, node, relational());
    } else {
      return node;
    }
  }
}

static Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<=")) {
      node = new_bin_node(ND_LTE, node, add());
    } else if (consume(">=")) {
      node = new_bin_node(ND_LTE, add(), node);
    } else if (consume("<")) {
      node = new_bin_node(ND_LT, node, add());
    } else if (consume(">")) {
      node = new_bin_node(ND_LT, add(), node);
    } else {
      return node;
    }
  }
}

static Node *new_add_node(Node *lhs, Node *rhs) {
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
    return new_bin_node(ND_ADD, lhs, rhs);
  } else if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
    // ptr + num のケース
    return new_bin_node(ND_PTR_ADD, lhs, rhs);
  } else if (is_integer(lhs->ty) && is_pointer(rhs->ty)){
    // num + ptrのケースなので、入れ替えてptr + num に正規化する
    return new_bin_node(ND_PTR_ADD, rhs, lhs);
  }
  // ここに来た場合 ptr + ptr という不正なパターンになるので落とす
  error_at(token->str, "不正なaddパターンです。");
  return NULL;
}

static Node *new_sub_node(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    // num - num
    return new_bin_node(ND_SUB, lhs, rhs);
  } else if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
    //  ptr - ptr
    return new_bin_node(ND_PTR_DIFF, lhs, rhs);
  } else if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
    // ptr - num
    return new_bin_node(ND_PTR_SUB, lhs, rhs);
  }
  // ここに来た場合 num - ptr になってしまうので、エラーにする
  error("不正な演算です: num - ptr");
  return NULL; // error で落ちるので実際にはreturnされない
}

static Node *add() {
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

static Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_bin_node(ND_MUL, node, unary());
    } else if (consume("/")) {
      node = new_bin_node(ND_DIV, node, unary());
    } else if (consume("%")) {
      node = new_bin_node(ND_MOD, node, unary());
    } else {
      return node;
    }
  }
}

static Node *unary() {
  if (consume("+")) {
    return postfix();
  } else if (consume("-")) {
    return new_bin_node(ND_SUB, new_num_node(0), postfix());
  } else if (consume("&")) {
    return new_unary_node(ND_ADDR, unary());
  } else if (consume("*")) {
    return new_unary_node(ND_DEREF, unary());
  } else if (consume_kind(TK_SIZEOF)) {
    Node *n = unary();
    add_type(n);
    return new_num_node(node_type_size(n));
  }
  return postfix();
}

Node *new_var_node(Var *var) {
  Node *node = new_node(ND_VAR);
  node->var = var;
  return node;
}

static Node *parse_var(Token *t) {
    Var *var = find_var(t);
    if (!var) {
      error_at(t->str, "変数 %s は宣言されていません。", my_strndup(t->str, t->len));
    }
    return new_var_node(var);
}

static Member *find_member(Type *type, char *name) {
  assert(type->kind == TY_STRUCT);

  for (Member *m = type->members; m ; m = m->next) {
    if (strcmp(m->name, name) == 0) {
      return m;
    }
  }
  return NULL;
}

static Node *struct_ref(Node *lhs) {
  add_type(lhs);

  if (lhs->ty->kind != TY_STRUCT) {
    error_at(token->str, "not a struct");
  }

  Token *tok = token;
  Member *m = find_member(lhs->ty, expect_ident());
  if (!m) {
    error_at(tok->str, "no such member");
  }

  Node *n = new_unary_node(ND_MEMBER, lhs);
  n->member = m;

  return n;
}

static Node *postfix() {
  Node *node = primary();
  for (;;) {
    if (consume("[")) {
      node = new_bin_node(ND_DEREF, new_add_node(node, expr()), NULL);
      expect("]");
      continue;
    }

    if (consume(".")) {
      node = struct_ref(node);
      continue;
    }
    break;
  }
  return node;
}

static Node *parse_call_func(Token *t) {
  Node *node = new_node(ND_CALL);
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

static char *next_data_label() {
  static int label_index = 0;
  char buf[100];
  int n = sprintf(buf, ".L.data.%03d", label_index++);
  return my_strndup(buf, n);
}

static Node *parse_string_literal(Token *str_token) {
  Type *ty = array_of(char_type, str_token->content_length);
  Var *var = new_gvar(next_data_label(), ty);
  var->contents = str_token->contents;
  var->content_length = str_token->content_length;

  return new_var_node(var);
}

static Node *primary() {
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
      // "("がなかったら変数参照
      return parse_var(t);
    }
  }

  t = consume_kind(TK_STR);
  if (t) {
    return parse_string_literal(t);
  }

  // () でも ident でもなければ 整数
  return new_num_node(expect_number());
}

static void free_nodes(Node *node) {
  if (!node) {
    return;
  }
  // Var の free は別途VarListのfreeで一緒にする(共有しているため)
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
  free_nodes(node->initializer);
  free(node->funcname);
  free(node);
}

static void free_lvar(Var *var) {
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

static void free_var_list_deep(VarList *var) {
  VarList *v = var;
  while (v) {
    VarList *tmp = v->next;
    free_lvar(v->var);
    v->var = NULL;
    free(v);
    v = tmp;
  }
}

static void free_var_list_shallow(VarList *var) {
  VarList *v = var;
  while (v) {
    VarList *tmp = v->next;
    free(v);
    v = tmp;
  }
}

static void free_function(Function *f) {
  free(f->name);
  // var_listの中のVarは locals にローカル変数も関数の引数も含めて全部もっているので localsのfreeで一緒に削除する
  free_var_list_deep(f->locals);
  // 引数のvar_listの Var * はlocalsのfreeでおこなｗれているので、こちらは VarList の freeのみ行う
  free_var_list_shallow(f->params);

  free_nodes(f->body);
  free(f);
}

static void free_functions(Function *func) {
  Function *f = func;
  while (f) {
    Function *tmp = f->next;
    free_function(f);
    f = tmp;
  }
}

void free_program(Program *program) {
  free_functions(program->functions);
  free_var_list_deep(program->global_var);
}

