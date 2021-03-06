#include "ynicc.h"
#include <stdlib.h>

/*
 * chibiccのスコープ管理の実装メモ
 *
 * locals ... 現在パース中の関数内のローカル変数のみを管理(ただし関数の仮引数もローカル変数として確保している)
 *            関数を管理しているFunction構造体には仮引数用のfunc->paramもあるが、これらの変数(Var型)も含めて重複して
 *            ローカル変数、仮引数まとめて全部このlocalsで管理されている。
 *            なぜかというと、ローカル変数用のスタック領域の確保時に、ローカル変数用＋仮引数用両方をみなくてもこのlocals
 *            で管理している分のサイズを全部合計してスタック領域として確保できて楽なため。
 *
 * globals ... 現在パース中のグローバル変数(たぶんstatic含む)を管理する。
 *
 * Scope ... 変数(ローカル、グローバル含む)と構造体のタグ名のスコープを管理する
 *           変数がlocalsと重複しているが、このScopeの方はなまえの通りScopeの管理もしていて、ブロックスコープの管理をするために、
 *           localsの中の各変数(＋グローバル変数) に関して、その時点でのスコープにある変数のみ保持している。
 *           なので、 locals >= Scopeのvar_scope という関係になる
 */

// 構造体とeumのタグ
typedef struct TagScope TagScope;
struct TagScope {
  TagScope *next;
  char *name; // タグ名
  Type *ty; // 構造体/enumの型自身
  int depth;
};

typedef struct VarScope VarScope;
struct VarScope {
  VarScope *next;
  char *name;

  // 変数の場合に設定
  Var *var;

  // typedefの場合のみ設定
  Type *type_def;

  // enumの場合のみ設定
  Type *enum_ty;
  int enum_val;

  int depth;
};

typedef struct Scope Scope;
struct Scope {
  VarScope *var_scope;
  TagScope *tag_scope;
};

typedef enum {
  TYPEDEF = 1 << 0,
  STATIC  = 1 << 1,
  EXTERN  = 1 << 2,
} StorageClass;

// 現在パース中のスコープにある変数(ローカル、グローバル含む)を管理する
static VarScope *var_scope;

// 現在パース中のスコープにある構造体のタグ名を管理する
static TagScope *tag_scope;

// 現在パース中のスコープの深さを保持
static int scope_depth;

// 今パース中の関数のローカル変数(+仮引数)
static VarList *locals = NULL;

// グローバル変数
static VarList *globals = NULL;

// 現在処理中のswitch文のノード(caseとdefaultをその後追加していく)
static Node *current_switch = NULL;

static Scope *enter_scope(void) {
  Scope *new_scope = calloc(1, sizeof(Scope));

  new_scope->var_scope = var_scope;
  new_scope->tag_scope = tag_scope;
  scope_depth++;

  return new_scope;
}

static void leave_scope(Scope *prev_scope) {
  var_scope = prev_scope->var_scope;
  tag_scope = prev_scope->tag_scope;
  scope_depth--;
}

static void push_tag_scope(Token *tag_tok, Type *type) {
  TagScope *sc = calloc(1, sizeof(TagScope));

  sc->name = my_strndup(tag_tok->str, tag_tok->len);
  sc->ty = type;
  sc->next = tag_scope;
  sc->depth = scope_depth;

  tag_scope = sc;
}

static VarScope *push_var_scope_helper(char *name) {
  VarScope *sc = calloc(1, sizeof(VarScope));

  sc->name = name;
  sc->next = var_scope;
  sc->depth = scope_depth;
  var_scope = sc;

  return sc;
}

static void push_var_scope(char *name, Var *var) {
  push_var_scope_helper(name)->var = var;

}

static void push_typedef_scope(char *name, Type *type_def) {
  push_var_scope_helper(name)->type_def = type_def;

  // fprintf(stderr, "push_typedef\n");
  // fprintf(stderr, "name: %s\n" ,name);
  // fprintf(stderr, "%s\n", type_info(type_def));
}

static void push_enum_scope(char *name, Type *enum_ty, int enum_val) {
  VarScope *sc = push_var_scope_helper(name);
  sc->enum_ty = enum_ty;
  sc->enum_val = enum_val;
}

static TagScope *find_tag(Token *tk) {
  for (TagScope *sc = tag_scope; sc; sc = sc->next) {
    if (strlen(sc->name) == tk->len && strncmp(sc->name, tk->str, tk->len) == 0) {
      // fprintf(stderr, "tag: %s, sc_name: %s\n", my_strndup(tk->str, tk->len), sc->name);
      return sc;
    }
  }
  return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node *new_bin_node(NodeKind kind, Node *lhs, Node *rhs, Token *tk) {
  Node *node = new_node(kind, tk);
  node->lhs = lhs;
  node->rhs = rhs;

  add_type(node);

  return node;
}

static Node *new_unary_node(NodeKind kind, Node *lhs, Token *tk) {
  Node *node = new_node(kind, tk);
  node->lhs = lhs;
  return node;
}

static Node *new_num_node(long val, Token *tk) {
  Node *node = new_node(ND_NUM, tk);
  node->val = val;

  return node;
}

static Var *new_var(char *name, Type *type, bool is_local) {
  Var *var = calloc(1, sizeof(Var));
  var->type = type;
  var->name = name;
  var->is_local = is_local;

  // 今のscopeにこの変数を追加
  push_var_scope(name, var);

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

static Var *new_gvar(char *name, Type *type, bool emit, bool is_static) {
  Var *var = new_var(name, type, false);
  var->is_static = is_static;
  if (emit) {
    // 通常のグローバル変数や、文字列リテラルは.dataセクションに出力するように
    // VarListとして保存するが、関数定義などのND_CALLのときのチェックようにしか使わないものは
    // (new_varの中でやっている)スコープにのみいれて、
    // emit: false で呼び出してコード生成時に何も出力しないようにする。
    VarList *v = calloc(1, sizeof(VarList));
    v->var = var;
    v->next = globals;
    globals = v;
  }

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

static Token *consume(char *op) {
  Token *tk = token;
  if (!peek_token(op)) {
    return NULL;
  }
  token = token->next;
  return tk;
}

static bool peek_kind(TokenKind kind) {
  if (token->kind == kind) {
    return true;
  }
  return false;
}

static Token *consume_kind(TokenKind kind) {
  if (!peek_kind(kind)) {
    return NULL;
  }

  Token *t = token;
  token = token->next;

  return t;
}

static void expect_kind(TokenKind kind) {
  if (!consume_kind(kind)) {
    error_at(token->str, "トークン %s がありません", token_kind_to_s(kind));
  }
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
    error_at(token->str, "'%c'(token->kind: %s, token->len: %d, op: %s)ではありません", op, token_kind_to_s(token->kind), token->len, op);
  }
  token = token->next;
}

static long expect_number() {
  if (token->kind != TK_NUM) {
    error_at(token->str, "数ではありません。");
  }
  long val = token->val;
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

// }か }, で終わると受理する
bool consume_end() {
  if (consume("}")) {
    return true;
  }

  Token *tmp = token;
  if (consume(",") && consume("}")) {
    return true;;
  }
  token = tmp;
  return false;
}

static void expect_end() {
  if (!consume_end())
    expect("}");
}

// }か }, で終わると受理する
bool peek_end() {
  Token *tmp = token;
  bool result = false;

  if (consume("}")) {
    result = true;
  } else if (consume(",") && consume("}")) {
    result =  true;;
  }

  token = tmp;
  return result;
}

static bool at_eof() {
  return token->kind == TK_EOF;
}

static VarScope *find_var(Token *token) {
  for (VarScope *sc = var_scope; sc; sc = sc->next) {
    if (sc->type_def) {
      // typedef は変数じゃないので無視
      continue;
    }

    if (strlen(sc->name) == token->len &&
        strncmp(sc->name, token->str, token->len) == 0) {
      return sc;
    }
  }
  return NULL;
}

static Type *find_typedef(Token *tk) {
  for (VarScope *sc = var_scope; sc; sc = sc->next) {
    if (sc->var) {
      // var はtypedefじゃないので無視
      continue;
    }

    assert(sc);
    if (strlen(sc->name) == tk->len &&
        strncmp(sc->name, tk->str, tk->len) == 0) {
      return sc->type_def;
    }
  }
  return NULL;
}

static bool is_builtin_type(Token *tk) {
  switch (tk->kind) {
    case TK_VOID:
    case TK_BOOL:
    case TK_INT:
    case TK_CHAR:
    case TK_LONG:
    case TK_SHORT:
      return true;
    default:
      return false;
  }
}

static bool is_type(Token *tk) {
  if (is_builtin_type(tk)) {
    return true;
  }

  // ビルトインでなくても、typedefか構造体かenumかtypedefされた型ならtrueを返す
  // 型の途中でtypedef/static/externが出てくる場合もあるのでtypedef/static/extern自体も型名の一つとしてtrueにしておき
  // 詳細はbasetype関数で判断する
  switch (tk->kind) {
    case TK_TYPEDEF:
    case TK_STRUCT:
    case TK_ENUM:
    case TK_STATIC:
    case TK_EXTERN:
      return true;
    default:
      return (bool)find_typedef(tk);
  }
}

// ynicc BNF
//
// program                   = (
//                               function_def_or_decl
//                             | global_var_decl
//                           )*
// basetype                  = ("void" | "_Bool" | "int" | "char" | "long" | "long" "long" | "short" | struct_decl | typedef_types | enum_decl)
// declarator                =  "*" * ( "(" declarator         ")" | ident )  type_suffix
// abstract_declarator       =  "*" * ( "(" abstract_declarator")"         )? type_suffix
// typename                  = basetype abstract_declarator type_suffix
// type_suffix               = ("[" const_expr? "]" type_suffix)?
// function_def_or_decl      = basetype declarator "(" function_params? ")" ("{" stmt* "}" | ";")
// global_var_decl           = basetype declarator ( "=" gvar_initializer )? ";"
// stmt                      = expr ";"
//                           | ";"
//                           | "{" stmt* "}"
//                           | "return" expr? ";"
//                           | "if" "(" expr ")" stmt ("else" stmt)?
//                           | "while" "(" expr ")" stmt
//                           | "do" stmt "while" "(" expr ")"
//                           | "for" "(" (expr | var_decl)? ";" expr? ";" expr? ")" stmt
//                           | "switch" "(" expr ")"
//                           | "case" const_expr ":" stmt
//                           | "default" ":" stmt
//                           | var_decl
//                           | "break" ";"
//                           | "continue" ";"
//                           | "goto" ident ";"
//                           | ident ":" stmt
// var_decl                  = basetype ( (var_decl_sub ("," var_decl_sub)*) | (static_var_decl_sub ("," static_var_decl)*) ) ";"
//                           | basetype ";"
// var_decl_sub              = declarator ( "=" local_var_initializer)?
// static_vart_decl          = declarator ( "=" gvar_initializer)?
// local_var_initializer     = local_var_initializer_sub
// local_var_initializer_sub = "{" local_var_initializer_sub ("," local_var_initializer_sub)* "}"
//                           | assign
// struct_decl               = "struct" ident? ("{" struct_member* "}") ?
// struct_member             = basetype declarator
// 
// 以降の演算子の優先順位は下記参照(この表で上にある演算子(優先度が高い演算子)ほどBNFとしては下にくる)
// http://www.bohyoh.com/CandCPP/C/operator.html
// expr                      = assign ("," assign)*
// assign                    = ternary ( ("="  | "+=" | "-=" | "*=" | "/=" | "|=" | "&=" | "^=") assign)?
// ternary                   = logical_or ("?" expr : expr)?
// const_expr                = eval( ternary )
// logical_or                = logical_and ( "||" logical_and )*
// logical_and               = bit_or ( "&&" bit_or )*
// bit_or                    = bit_and ( "|" bit_and )*
// bit_xor                   = bit_and ( "&" bit_and )*
// bit_and                   = equality ( "^" equality )*
// equality                  = relational ("==" relational | "!=" relational)*
// relational                = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
// shift                     = add ("<<" add | ">>" add)*
// add                       = mul ("+" mul | "-" mul)*
// mul                       = cast ("*" cast | "/" cast)*
// cast                      = "(" typename ")" cast | unary
// unary                     = "+"? cast
//                           | "-"? cast
//                           | "&" cast
//                           | "*" cast
//                           | "++" cast
//                           | "--" cast
//                           | "!" cast
//                           | "sizeof" cast
//                           | "sizeof" "(" type_name ")"
//                           | "_Alignof "(" type_naem ")"
//                           | postfix
// postfix                   = ccompound-literal
//                           | parimary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
// compound_literal         = "(" type_name ")" "{" (gvar_initializer | local_var_initializer) "}"
// primary                   = num
//                           | ident ("(" arg_list? ")")?
//                           | "(" expr ")"
//                           | str

Program *program();

static Function *function_def_or_decl();
static Node *stmt();
static Type *basetype(StorageClass *sclass);
static Type *declarator(Type *ty, char **name);
static Type *abstract_declarator(Type *ty);
static Node *var_decl();
static Node *var_decl_sub();
static void static_var_decl_sub(Type *base, StorageClass sclass);
static Node *expr();
static Node *assign();
static Node *ternary();
static long const_expr();
static long eval(Node *node);
static long eval2(Node *node, Var **var);
static Node *bit_and();
static Node *logical_or();
static Node *logical_and();
static Node *bit_or();
static Node *bit_xor();
static Node *equality();
static Node *relational();
static Node *shift();
static Node *add();
static Node *new_add_node(Node *lhs, Node *rhs, Token *tk);
static Node *new_sub_node(Node *lhs, Node *rhs, Token *tk);
static Node *mul();
static Node *cast();
static Node *unary();
static Node *postfix();
static Node *compound_literal();
static Node *primary();
static Node *read_expr_stmt();
static Type *type_suffix(Type *base);
static Type *struct_decl();
static Type *enum_decl();
static char *new_label();

typedef enum {
  NEXT_DECL_DUMMY,
  NEXT_DECL_FUNCTION_DEF,
  NEXT_DECL_GLOBAL_VAR_OR_TYPEDEF,
} next_decl_type;

static next_decl_type next_decl() {
  // 先読みした結果今の位置に戻る用に今のtokenを保存
  Token *tmp = token;
  bool is_func = false;
  bool is_global_var = false;

  StorageClass sclass = 0;
  Type *ty = basetype(&sclass);

  if (sclass == TYPEDEF) {
    // typedefがあったら、それは関数ではないとみなす
    token = tmp;
    return NEXT_DECL_GLOBAL_VAR_OR_TYPEDEF;
  }

  // toplevelでは
  // int;
  // のような式も合法で、これらは単に無視される。ので、その対応。
  if (!consume(";")) {
    char *name = NULL;
    declarator(ty, &name);
    if (consume("(")) {
      token = tmp;
      return NEXT_DECL_FUNCTION_DEF;
    }
  }
  // 先読みした結果、関数定義かグローバル変数かわかったので、tokenを元に戻す
  token = tmp;
  // それ以外はグローバル変数
  return NEXT_DECL_GLOBAL_VAR_OR_TYPEDEF;
}

static void parse_typedef() {
  Type *ty = basetype(NULL);
  char *name;
  ty = declarator(ty, &name);
  expect(";");
  push_typedef_scope(name, ty);
}

static Initializer *new_init_val(Initializer *cur, int sz, long val) {
  Initializer *initializer = calloc(1, sizeof(Initializer));
  initializer->val = val;
  initializer->sz = sz;
  cur->next = initializer;
  return initializer;
}

// addendは labelからのオフセットを指定するときに渡される
static Initializer *new_init_label(Initializer *cur, char *label, long addend) {
  Initializer *initializer = calloc(1, sizeof(Initializer));
  initializer->label = label;
  initializer->addend = addend;
  cur->next = initializer;
  return initializer;
}

static Initializer *new_init_string(char *p, int len) {
  Initializer head = {};
  Initializer *cur = &head;
  for (int i = 0; i < len; i++) {
    cur = new_init_val(cur, 1, p[i]);
  }
  return head.next;
}

static Initializer *new_init_zero(Initializer *cur, int nbytes) {
  for (int i = 0; i < nbytes; i++) {
    cur = new_init_val(cur, 1, 0);
  }
  return cur;
}

// global変数として今パース中の構造体を割り付けるにあたって、
// 各メンバーの間にあるパディングに0を埋めるためのInitializeを確保する
// 例)
// struct {
//   char a;
//   int  b;
// } x;
// というグローバル変数xがある場合、この構造体のメモリ上でのレイアウトは下記のようになる
//      +--+
// x -> |  | <- char a のアドレス(1バイト, xからのoffset 0)
//      +--+
//      |  | -+-- A
//      +--+  |
//      |  |  |
//      +--+  |
//      |  | -+-- B このAからBまでの3byteはアラインメントによるパディングで、ここをゼロで初期化するためのinitializerを生成するのがこの関数
//      +--+
//      |  | <- int b のアドレス(4バイト, xからのoffset 4)
//      +--+  |
//      |  |  |
//      +--+  |
//      |  |  |
//      +--+  |
//      |  |  |
//      +--+  <--ここまでがint bの領域
//
// この関数の前提として、 引数のcurがあるメンバーを読んだ直後のInitializerになるので、
// 例えば、 char aの初期化式を読んだ場合は、その初期化式を設定するinitializerがcurとして渡ってくる
static Initializer *emit_struct_padding(Initializer *cur, Type *parent, Member *mem) {
  // padding領域の開始位置
  int a = mem->offset + mem->ty->size;
  // padding領域の終了位置(最後のメンバーの場合、構造体の最後の領域までをパッディングとする
  int b = mem->next ? mem->next->offset : parent->size;
  int padding_size = b - a; // a, b はそれぞれ上の説明の A, Bのこと。例でいうと b = 4, a = (0 + 1) = 1 となって b - a が 3になり3バイト分ゼロで初期化する
  cur = new_init_zero(cur, padding_size);
  return cur;
}

static bool is_array_limit_over(Type *ty, int i) {
  if (ty->is_incomplete) {
    // サイズが不定の場合は無制限にする
    return false;
  }
  return i >= ty->array_size;
}

static void skip_excess_elements2() {
  for (;;) {
    if (consume("{")) {
      skip_excess_elements2();
    } else {
      // brace以外の場合は何か式があるはずなので読み飛ばす
      assign();
    }
    if (consume_end()) {
      return;
    }
    expect(",");
  }
}

static void skip_excess_elements(void) {
  expect(",");
  fprintf(stderr, "省略された初期化式があります\n");
  skip_excess_elements2();
}

static Initializer *gvar_initializer_sub(Initializer *cur, Type *ty) {
  Token *cur_tok = token;

  if (ty->kind == TY_ARRAY && ty->ptr_to->kind == TY_CHAR) {
    Token *str = consume_kind(TK_STR);
    if (str) {
      if (ty->is_incomplete) {
        ty->array_size = ty->size = str->content_length;
        ty->is_incomplete = false;
      }

      int len = str->content_length > ty->array_size ? ty->array_size : str->content_length;
      for (int i = 0; i < len; i++) {
        cur = new_init_val(cur, 1, str->contents[i]);
      }
      for (; len < ty->array_size; len++) {
        cur = new_init_val(cur, 1, 0);
      }

      return cur;
    }
  }

  if (ty->kind == TY_ARRAY) {
    bool has_open_brace = consume("{");
    int i = 0;
    if (!peek_token("}")) {
      do {
        cur = gvar_initializer_sub(cur, ty->ptr_to);
        i++;
      } while(!is_array_limit_over(ty, i) && !peek_end() && consume(","));

      if (has_open_brace && !consume_end()) {
        // "{" で始まってるのに、配列のサイズ個の初期化式を読んでもなおまだ初期化式が残っている場合、無視する
        skip_excess_elements();
      }
    }
    if (i < ty->array_size) {
      for (; i < ty->array_size; i++) {
        cur = new_init_zero(cur, ty->ptr_to->size);
      }
    }

    if (ty->is_incomplete) {
      ty->array_size = i;
      ty->size = ty->ptr_to->size * i;
      ty->is_incomplete = false;
    }

    return cur;
  }

  if (ty->kind == TY_STRUCT) {
    bool has_open_brace = consume("{");
    if (!peek_token("}")) {
      Member *mem = ty->members;
      do {
        cur = gvar_initializer_sub(cur, mem->ty);
        cur = emit_struct_padding(cur, ty, mem);
        mem = mem->next;
      } while(mem && !peek_end() && consume(","));

      if (has_open_brace && !consume_end()) {
        // "{" で始まってるのに、配列のサイズ個の初期化式を読んでもなおまだ初期化式が残っている場合、無視する
        skip_excess_elements();
      }

      if (mem) {
        for (; mem; mem = mem->next) {
          cur = new_init_zero(cur, mem->ty->size);
        }
      }
    }
    return cur;
  }


  // compound-literal で "{" 初期化式 "}" で初期化する場合に "{" が存在する
  // (int) { 1 } とか
  bool has_open_brace = consume("{");
  Node *expr = ternary();
  if (has_open_brace) {
    expect_end();
  }

  // グローバル変数の初期化で別のグローバル変数のアドレス参照のみ使えるため、
  // 初期化式の中に最大一つだけ変数を取れる。
  // その変数をevalの評価中に合わせて取得し返してもらう。
  // その初期化式の値は、 evalの中で見つかった別のグローバル変数を基準にしたアドレス計算の結果をinitializerとする
  Var *var = NULL;
  long addend = eval2(expr, &var);
  if (var) {
    // 変数からのオフセットの初期化式だったので、ポインタ演算とみなして実際のバイト数に変換する
    int scale = (var->type->kind == TY_ARRAY) ? var->type->ptr_to->size : var->type->size;
    return new_init_label(cur, var->name, addend * scale);
  }

  // 変数がない場合は単にevalの値を定数として設定するinitializerとする
  return new_init_val(cur, ty->size, addend);
}

static Initializer *gvar_initializer(Type *ty) {
  Initializer head;
  gvar_initializer_sub(&head, ty);
  return head.next;
}

// グローバル変数のパース
static void global_var_decl() {
  StorageClass sclass = false;
  Type *type = basetype(&sclass);

  if (consume(";")) {
    // int; とかの場合
    return;
  }

  char *ident_name;
  Token *tk = token;
  type = declarator(type, &ident_name);

  if (sclass == TYPEDEF) {
    expect(";");
    push_typedef_scope(ident_name, type);
    return;
  }

  // extern が設定sれていない場合はこのファイルで領域を確保する
  // それ以外はどこかのファイルのコンパイルで領域が確保されているはずなので、名前のみ登録
  Var *var = new_gvar(ident_name, type, sclass != EXTERN, sclass == STATIC);
  if (sclass == EXTERN) {
    // extern の場合初期化式は書けず、宣言のみになる
    expect(";");
    return;
  }
  if (!consume("=")) {
    if (type->is_incomplete) {
      error_at(tk->str, "incomplete element type(gvar)");
    }
    expect(";");
    return;
  }
  var->initializer = gvar_initializer(type);
  expect(";");
}


Program *program() {
  Function head = {};
  Function *cur = &head;
  while (!at_eof()) {
    switch(next_decl()) {
      case NEXT_DECL_FUNCTION_DEF:
        {
          Function *f = function_def_or_decl();
          if (f) {
            cur->next = f;
            cur = cur->next;
          }
        }
        break;
      case NEXT_DECL_GLOBAL_VAR_OR_TYPEDEF:
        global_var_decl();
        break;
      default:
        // ここには来ないはず
        assert(false);
    }
  }
  Program *program = calloc(1, sizeof(Program));
  program->global_var = globals;
  program->functions = head.next;

  return program;
}

// cの宣言
// - typedefは何回書いても1個とみなされる。またtypedefしたい型名を省略した場合intとしてtypedefされる。
//   - 下記は全部合法で、 hogeをint型としてtypedefする
//   typedef typedef int hoge;
//   typedef typedef hoge
//   int typedef hoge;
//   int typedef typdef hoge;
//
// このため、変数の宣言をパースしきらないとtypedefかどうかもわからないので、今パースしようとしている場所がtypedef可能かどうかを引数のsclassで渡してもらう。
// その上で、本当にtypedefがあったか、なかったか(=普通の変数宣言だったか)を sclassに設定して返す
// 一方、例えば関数の戻り値の型部分などのようにtypedefが存在しできない場合は sclass をNULLとして渡してもらって判断する。
//
// また、intやlongのように何度か型宣言に含められるものがあり、
// それらの順番まですべてを想定すると大変なので、最大何個まで各型を書けるかの数で判断する方針らしい
// - short型
//   short
//   short int
//   int short
//   => shortがある場合はintが1個あっても良い
// - long型
//   long
//   long long
//   long long int
//   long int long
//   int long long
//   => longがある場合は longかintがそれぞれ最大1個あっても良い
//
static Type *basetype(StorageClass *sclass) {
  if (!is_type(token)) {
    error_at(token->str, "typename expected");
  }

  // typedef x
  // なども合法なのでデフォルトの型をintにしておく
  Type *ty = int_type;

  // 各型が何回出てきたかを数えるcounter
  int counter = 0;

  // counterに加えていく値。
  // 適当に2回足してもかぶらないようにシフトさせているんだと思う
  enum {
    VOID  = 1 << 0,
    BOOL  = 1 << 2,
    CHAR  = 1 << 4,
    SHORT = 1 << 6,
    INT   = 1 << 8,
    LONG  = 1 << 10,
    OTHER = 1 << 12,
  };

  // storage class が指定可能な場所はまず未定義で初期化
  if (sclass) {
    *sclass = 0;
  }

  while (is_type(token)) {
    Token *tk = token;

    if (peek_kind(TK_TYPEDEF) || peek_kind(TK_STATIC) || peek_kind(TK_EXTERN)) {
      if (!sclass) {
        error_at(tk->str, "ストレージクラス指定子はここでは使えません");
      }
      // 一度でもどこかにtypedef/staticがついていたら覚えておく
      if (consume_kind(TK_TYPEDEF)) {
        *sclass |= TYPEDEF;
      } else if (consume_kind(TK_STATIC)) {
        *sclass |= STATIC;
      } else if (consume_kind(TK_EXTERN)) {
        *sclass |= EXTERN;
      }

      // typedef と static と extern を同時に使っていたらエラーにする
      // 001        010       100
      // 3bitのうちどこか1bitのみ立っている場合だけOKでそれ以外はNGにしたい。
      // ある値xがあったとして
      // x & (x - 1) が 0 になる場合は どこかの1bitしか立っていないということに鳴るらしい
      // 例
      // x = 0b0010 の場合
      // x & (x - 1) = 0b0010 & 0b0001 = 0
      //
      // x = 0b0011 の場合
      // x & (x - 1) = 0b0011 & 0b0001 = 0b0001
      //
      // となり、複数のbitが立っているときのみ0以外になる
      if (*sclass & (*sclass - 1)) {
        // どこかの複数のbitが立っていた場合は複数のsclassが使われていることになるので
        error_at(tk->str, "static、typedefとexternは同時に指定できません");
      }

      continue;
    }

    if (!is_builtin_type(tk)) {
      if (counter) {
        //ビルトインの型名じゃないものがきて、かつ、一度でも何かの型を読んでいたら
        //変数宣言(またはtypedef)の後の識別子にきてるので、終了
        break;
      }
      //まだ何も型名を読んでいなくて、ビルトインの方じゃない場合は、構造体か、typedefされた型名になる。
      if (tk->kind == TK_STRUCT) {
        ty = struct_decl();
      } else if (tk->kind == TK_ENUM) {
        ty = enum_decl();
      } else {
        ty = find_typedef(tk);
        // fprintf(stderr, "found typedef: %s\n", my_strndup(tk->str, tk->len));
        // fprintf(stderr, "found is_incomplete: %d\n", ty->is_incomplete);
        assert(ty);
        token = token->next;
      }

      // ビルトイン以外の型を読んだのでカウンターを上げる
      counter += OTHER;
      continue;
    }

    if (consume_kind(TK_VOID)) {
      counter += VOID;
    } else if (consume_kind(TK_BOOL)) {
      counter += BOOL;
    } else if (consume_kind(TK_CHAR)) {
      counter += CHAR;
    } else if (consume_kind(TK_SHORT)) {
      counter += SHORT;
    } else if (consume_kind(TK_INT)) {
      counter += INT;
    } else if (consume_kind(TK_LONG)) {
      counter += LONG;
    }

    // counterがとり得る値のみ許可して他はエラー
    switch (counter) {
      case VOID:
        ty = void_type;
        break;
      case BOOL:
        ty = bool_type;
        break;
      case CHAR:
        ty = char_type;
        break;
      case SHORT:
      case SHORT + INT: // これで short int, int short どっちも判断できる
        ty = short_type;
        break;
      case INT:
        ty = int_type;
        break;
      case LONG:
      case LONG + INT:
      case LONG + LONG:
      case LONG + LONG + INT:
        ty = long_type;
        break;
      default:
        error_at(tk->str, "不正な型です");
    }
  }

  return ty;
}
/**
 * このパース方法に関しては、コンパイラブックの 「Cの型の構文」
 * https://www.sigbus.info/compilerbook#type
 * を参照。
 * とくに、placeholderの部分などは、この説の説明を見ないと難しい。
 */
static Type *declarator(Type *ty, char **name) {
  while (consume("*")) {
    ty = pointer_to(ty);
  }
  if (consume("(")) {
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty = declarator(placeholder, name);
    expect(")");
    // 入れ子部分を全部パースした後に、その後に続く配列の [] などを含めた(type_suffix)型としてplaceholderを完成させる
    memcpy(placeholder, type_suffix(ty), sizeof(Type));
    return new_ty;
  }

  *name = expect_ident();
  return type_suffix(ty);
}

// declarator の識別子が無い版
static Type *abstract_declarator(Type *ty) {
  while (consume("*")) {
    ty = pointer_to(ty);
  }
  if (consume("(")) {
    Type *placeholder = calloc(1, sizeof(Type));
    Type *new_ty = abstract_declarator(placeholder);
    expect(")");
    // 入れ子部分を全部パースした後に、その後に続く配列の [] などを含めた(type_suffix)型としてplaceholderを完成させる
    memcpy(placeholder, type_suffix(ty), sizeof(Type));
    return new_ty;
  }

  return type_suffix(ty);
}

static Type *type_name(void) {
  Type *ty = basetype(NULL);
  ty = abstract_declarator(ty);
  return type_suffix(ty);
}

static Member *struct_member() {
  Type *type = basetype(NULL);
  char *ident;
  Token *tk = token;
  type = declarator(type, &ident);
  if (type->is_incomplete) {
    error_at(tk->str, "incomplete element type(struct_member)");
  }
  expect(";");

  Member *m = calloc(1, sizeof(Member));
  m->name = ident;
  m->ty = type;

  return m;
}

static Type *struct_decl() {
  expect_token(TK_STRUCT);

  Token *bak = token;
  Token *tk = consume_ident();

  // 構造体のtagのみ宣言（？）
  // struct hoge;
  //
  // その後実際の定義を宣言
  // struct hoge {
  //   int x;
  // }
  // みたいなことができるので、 struct hoge; のような場合はタグ名だけ登録する
  if (tk) {
    if (!peek_token("{")) {
      TagScope *sc = find_tag(tk);
      if (!sc) {
        // 無かったらこのタグ名で登録する
        // (登録はするがまだこの時点では完全な定義でないので、ty->is_incompleteがtrueのままになる)
        Type *ty = struct_type();
        push_tag_scope(tk, ty);
        return ty;
      }
      if (sc->ty->kind != TY_STRUCT) {
        error_at(bak->str, "%s は構造体ではありません", my_strndup(tk->str, tk->len));
      }
      return sc->ty;
    }
  }

  // ここに来た場合 構造体の宣言の struct _ で(もしタグがあればそれの後) _ をこれから読もうとしている
  // chibiccを見ると struct *foo というのもcの文法としてはOKで、「不完全な無名構造体へのポインタ」という変数を定義したことになるらしい。
  // なので、 「struct」 まででその「不完全な無名構造体」とみなしてその型を返すらしい。
  if (!consume("{")) {
    return struct_type();
  }

  Type *ty = NULL;

  TagScope *sc = NULL;
  if (tk) {
    sc = find_tag(tk);
  }

  if (sc && sc->depth == scope_depth) {
    if (sc->ty->kind != TY_STRUCT) {
      error_at(tk->str, "%s は構造体ではありません", my_strndup(tk->str, tk->len));
    }
    // ここに来た場合、不完全型で定義した構造体の定義になるのでその不完全型を設定して、
    // ここ以下のメンバーのパース後に完全型になる
    ty = sc->ty;
  } else {
    // 新規のタグの場合
    // struct tag {
    //   struct tag *next
    // }
    // というような、メンバーの型が自分の構造体のタグのようなケースのために、
    // メンバーのパースの前にタグを登録しておく
    ty = struct_type();
    if (tk) {
      push_tag_scope(tk, ty);
    }
  }

  Member head = {};
  Member *cur = &head;

  while (!consume("}")) {
    cur->next = struct_member();
    cur = cur->next;
  }

  ty->members = head.next;

  int offset = 0;
  for (Member *m = ty->members; m; m = m->next) {
    offset = align_to(offset, m->ty->align);
    m->offset = offset;
    offset += m->ty->size;
    if (ty->align < m->ty->align) {
      // 構造体自身のアラインメントは、構造体のメンバーの中の最大のアラインメントに合わせる
      // 構造体自体のサイズの割当(ループの外)で効いてくるらしい
      ty->align = m->ty->align;
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
  ty->size = align_to(offset, ty->align);

  // メンバーの定義までパースして初めて不完全型->完全型になる
  ty->is_incomplete = false;

  return ty;
}

static Type *enum_decl() {
  expect_token(TK_ENUM);

  Token *bak = token;

  Token *tag_tk = consume_ident();

  if (tag_tk) {
    if (!peek_token("{")) {
      // 構造体と同じくenum tagの次に"{"がない定義済みenum型の変数宣言になるので、tag名で探す
      TagScope *sc = find_tag(tag_tk);
      if (!sc) {
        error_at(bak->str, "enum: %s が見つかりません", my_strndup(tag_tk->str, tag_tk->len));
      }
      if (sc->ty->kind != TY_ENUM) {
        error_at(bak->str, "%s はenumではありません", my_strndup(tag_tk->str, tag_tk->len));
      }
      return sc->ty;
    }
  }
  // ここに来た場合は新規のenumの定義
  expect("{");
  Type *ty = enum_type();
  int enum_value = 0;
  for (;;) {
    char *ident = expect_ident();
    if (consume("=")) {
      enum_value = const_expr();
    }
    push_enum_scope(ident, ty, enum_value++);

    if (consume_end()) {
      break;
    }
    expect(",");
  }

  if (tag_tk) {
    // enumのタグがあったらそのtag名で登録
    push_tag_scope(tag_tk, ty);
  }

  return ty;;
}

static void function_params(Function *func) {
  if (consume(")")) {
    return;
  }

  Token *tmp = token;
  if (consume_kind(TK_VOID) && consume(")")) {
    return;
  }
  token = tmp;

  Type *type = basetype(NULL);
  char *name;
  type = declarator(type, &name);
  if (type->kind == TY_ARRAY) {
    // 関数の仮引数で宣言されている配列型のみ、ポインタに読み替えて受けるようにする
    type = pointer_to(type->ptr_to);
  }
  Var *var = new_lvar(name, type);
  VarList *var_list = calloc(1, sizeof(VarList));
  var_list->var = var;
  // fprintf(stderr, "parse func param start\n");
  while (!consume(")")) {
    expect(",");
    if (consume("...")) {
      func->has_vararg = true;
      // ... は引数の最後であること
      expect(")");
      break;
    }
    type = basetype(NULL);
    char *name;
    type = declarator(type, &name);
    if (type->kind == TY_ARRAY) {
      // 関数の仮引数で宣言されている配列型のみ、ポインタに読み替えて受けるようにする
      type = pointer_to(type->ptr_to);
    }
    Var *var = new_lvar(name, type);
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
  // vaargの場合引数のregister、6個を保存する場所＋ ... までの引数の個数情報をわたす1個で合計 (6+1)*8 = 56byteのオフセットを準備
  int offset = f->has_vararg ? 56 : 0;
  for (VarList *v = f->locals; v; v = v->next) {
    Var *var = v->var;
    offset = align_to(offset, var->type->align);
    offset += var->type->size;
    var->offset = offset;
  }
  f->stack_size = align_to(offset, 8);
}

static Function *function_def_or_decl() {
  // 今からパースする関数ようにグローバルのlocalsを初期化
  locals = NULL;

  StorageClass sclass;
  Type *ret_type = basetype(&sclass);
  char *ident;
  ret_type = declarator(ret_type, &ident);
  Function *func = calloc(1, sizeof(Function));
  func->return_type = ret_type;
  func->name = ident;
  // 関数呼び出し時のチェック用に定義した関数も関数型としてscopeに入れる
  // is_staticに関して、関数は関数自体で別途is_staticを持っていて、そちらで .global の出力有無を制御しているので、このgvarのis_statciは何でもよい
  new_gvar(func->name, func_type(func->return_type), false, false);

  Scope *sc = enter_scope();
  expect("(");
  function_params(func);

  // ")" の後に ; が来てたら関数の定義なしで宣言のみ
  if (consume(";")) {
    leave_scope(sc);
    return NULL;
  }
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
  func->is_staitc = (sclass == STATIC);
  set_stack_info(func);

  leave_scope(sc);
  return func;
}

static Node *read_expr_stmt() {
  Node *node = new_node(ND_EXPR_STMT, token);
  node->kind = ND_EXPR_STMT;
  node->lhs = expr();
  return node;
}

static Node *stmt() {
  Token *tk;
  Node *node = NULL;

  if (tk = consume(";")) {
    node = new_node(ND_NULL, tk);
  } else if (tk = consume_kind(TK_RETURN)) {
    node = new_node(ND_RETURN, tk);
    if (!consume(";")) {
      node->lhs = expr();
      expect(";");
    }
  } else if (tk = consume_kind(TK_IF)) {
    node = new_node(ND_IF, tk);

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
  } else if (tk = consume_kind(TK_WHILE)) {
    node = new_node(ND_WHILE, tk);

    expect("(");
    node->cond = expr();
    expect(")");
    node->body = stmt();
  } else if (tk = consume_kind(TK_DO)) {
    node = new_node(ND_WHILE, tk);
    node->is_do_while = true;
    node->body = stmt();
    expect_kind(TK_WHILE);
    expect("(");
    node->cond = expr();
    expect(")");
    expect(";");
  } else if (tk = consume_kind(TK_FOR)) {
    node = new_node(ND_FOR, tk);
    Scope *sc = enter_scope();
    expect("(");
    if (consume(";")) {
      //次が";"なら初期化式なしなのでNULL入れる
      node->init = NULL;
    } else {
      // ";"でないなら初期化式かまたは辺宣言あるのでパース
      if (is_type(token)) {
        node->init = var_decl();
      } else {
        node->init = read_expr_stmt();
        expect(";");
      }
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
    leave_scope(sc);
  } else if (tk = consume("{")) {
    node = new_node(ND_BLOCK, tk);
    Scope *sc = enter_scope();
    int i = 0;
    Node head = {};
    Node *cur = &head;
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    node->body = head.next;
    leave_scope(sc);
  } else if (tk = consume_kind(TK_BREAK)) {
    expect(";");
    node = new_node(ND_BREAK, tk);
  } else if (tk = consume_kind(TK_CONTINUE)) {
    expect(";");
    node = new_node(ND_CONTINUE, tk);
  } else if (tk = consume_kind(TK_GOTO)) {
    char *ident = expect_ident();
    expect(";");
    node = new_node(ND_GOTO, tk);
    node->label_name = ident;
  } else if (tk = consume_kind(TK_SWITCH)) {
    expect("(");
    Node *switch_condition = expr();
    expect(")");
    node = new_unary_node(ND_SWITCH, switch_condition, tk);

    // 今のswitchを保存して現在のswitchに切り替える
    Node *tmp = current_switch;
    current_switch = node;
    node->body = stmt();
    // もとのswitchに戻す
    current_switch = tmp;
  } else if (tk = consume_kind(TK_CASE)) {
    if (!current_switch) {
      error(tk->str, "対応するswitchがありません");
    }
    int case_cond_val = const_expr();
    expect(":");
    node = new_unary_node(ND_CASE, stmt(), tk);
    node->case_cond_val = case_cond_val;
    node->is_default_case = false;
    node->case_next = current_switch->case_next;
    current_switch->case_next = node;
  } else if (tk = consume_kind(TK_DEFAULT)) {
    if (!current_switch) {
      error(tk->str, "対応するswitchがありません");
    }
    expect(":");
    node = new_unary_node(ND_CASE, stmt(), tk);
    node->is_default_case = true;
    current_switch->default_case = node;
  } else {
    Token *tmp = token;
    Token *tk = consume_ident();
    if (consume(":")) {
      // identの次に":" が来てたらラベル
      node = new_unary_node(ND_LABEL, stmt(), tk);
      node->label_name = my_strndup(tk->str, tk->len);
    } else {
      // ラベルじゃなかったので、もとに戻す
      token = tmp;
      // キーワードじゃなかったら 変数宣言かどうかチェック
      node = var_decl();
      if (!node) {
        // 変数宣言でなかったら式文
        node = read_expr_stmt();
        expect(";");
      }
    }
  }

  add_type(node);
  return node;
}

// int *x[n]
// のxの直後の[n]をパースする
static Type *type_suffix(Type *base) {
  if (!consume("[")) {
    return base;
  }

  int array_size = 0;
  int is_incomplete = true;
  if (!consume("]")) {
    //[の直後に"]"が来ていない場合、配列のサイズがあるはずなので、完全な方としてis_incompleteをfalseに
    is_incomplete = false;
    array_size = const_expr();
    expect("]");
  }

  Token *tk = token;
  base = type_suffix(base);
  // 不完全型許されるのは最初の配列の最初の次元のみなので、再帰で呼ばれた結果(=2次元以降)に関しては必ず完全になっていること
  if (base->is_incomplete) {
    error_at(tk->str, "incomplete element type(type_suffix)");
  }
  base = array_of(base, array_size);
  base->is_incomplete = is_incomplete;

  return base;
}

// https://github.com/rui314/chibicc/blob/e1b12f2c3d0e4389f327fcaa7484b5e439d4a716/parse.c#L679
// 配列の初期化式の各要素の位置を表す構造体
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
  int idx;     // 配列の初期化式の場合
  Member *mem; // 構造体の初期化式の場合
};

static Node *new_desg_node_sub(Var *var, Designator *desg, Token *tk) {
  if (!desg) {
    return new_var_node(var, tk);
  }
  Node *node = new_desg_node_sub(var, desg->next, tk);
  if (desg->mem) {
    // dsegが構造体のメンバー参照なら、返ってきたnodeを構造体の変数とみなしてそのメンバー参照のastを作成
    node = new_unary_node(ND_MEMBER, node, tk);
    node->member = desg->mem;

    return node;
  }

  node = new_add_node(node, new_num_node(desg->idx, tk), tk);

  node = new_unary_node(ND_DEREF, node, tk);
  add_type(node);

  return node;
}

static Node *new_desg_node(Var *var, Designator *desg, Node *rhs) {
  Node *lhs = new_desg_node_sub(var, desg, rhs->tok);
  Node *assign = new_bin_node(ND_ASSIGN, lhs, rhs, rhs->tok);
  return new_unary_node(ND_EXPR_STMT, assign, rhs->tok);
}

static Node *lvar_init_zero(Node *cur, Var *var, Type *ty, Designator *dseg) {
  if (ty->kind == TY_ARRAY) {
    // 配列の場合は各要素も再帰的に0にする
    for (int i = 0; i < ty->array_size; i++) {
      Designator desg2 = {dseg, i};
      cur = lvar_init_zero(cur, var, ty->ptr_to, &desg2);
    }
    return cur;
  }
  // 配列でない場合は、単純にその変数(入れ子の配列の場合はどこかの末端の要素)に0を代入するコードをnextにつなげていく
  cur->next = new_desg_node(var, dseg, new_num_node(0, token));
  return cur->next;
}

static void array_size_completed(Type *ty, int array_size) {
  ty->is_incomplete = false;
  ty->array_size = array_size;
  ty->size = ty->ptr_to->size * ty->array_size;
}

static Node *local_var_initializer_sub(Node *cur, Var *var, Type *ty, Designator *desg) {
  if (ty->kind == TY_ARRAY && ty->ptr_to->kind == TY_CHAR && peek_kind(TK_STR)) {
    // char hoge[3] = "abc" のような場合
    // hoge[0] = 'a';
    // hoge[1] = 'b';
    // hoge[2] = 'c';
    // のようなコードを生成する
    // gccでためしたところ
    // char hoge[10] = "01234";
    // みたいに配列の長さに満たない分は0で初期化されていいるっぽい
    // 初期化敷なしで宣言だけの(char hoge[10];)場合は値は不定
    Token *str = consume_kind(TK_STR);

    int len = ty->array_size > str->content_length ? str->content_length : ty->array_size;
    for (int i = 0; i < len; i++) {
      Designator desg2 = {desg, i};
      cur->next = new_desg_node(var, &desg2, new_num_node(str->contents[i], str));
      cur = cur->next;
    }

    for (int i = len; i < ty->array_size; i++) {
      Designator desg2 = {desg, i};
      cur = lvar_init_zero(cur, var, ty->ptr_to, &desg2);
    }
    // 不完全型(配列のサイズが省略されている場合)の場合、初期化した文字列の長さに合わせる
    if (ty->is_incomplete) {
      array_size_completed(ty, str->content_length);
    }

    return cur;
  }


  // static int count = 0;
  // fprintf(stdout, "count: %d, type: %d\n", count++, ty->kind);
  if (ty->kind == TY_ARRAY) {
    bool has_open_brace = consume("{");
    int i = 0;

    if (!peek_token("}")) {
      do {
        Designator desg2 = {desg, i++};
        cur = local_var_initializer_sub(cur, var, ty->ptr_to, &desg2);
      } while(!is_array_limit_over(ty, i) && !peek_end() && consume(","));
    }

    if (has_open_brace && !consume_end()) {
      // "{" で始まってるのに、配列のサイズ個の初期化式を読んでもなおまだ初期化式が残っている場合、無視する
      skip_excess_elements();
    }

    // 初期化子が配列のサイズに満たない場合は、0で初期化する
    for (; i < ty->array_size; i++) {
      Designator dseg2 = {desg, i};
      cur = lvar_init_zero(cur, var, ty->ptr_to, &dseg2);
    }

    // 不完全型(配列のサイズが省略されている場合)の場合、初期化した文字列の長さに合わせる
    if (ty->is_incomplete) {
      array_size_completed(ty, i);
    }

    return cur;
  }

  if (ty->kind == TY_STRUCT) {
    bool has_open_brace = consume("{");
    Member *mem = ty->members;

    if (!peek_token("}")) {
      do {
        assert(mem);
        Designator desg2 = {desg, 0, mem};
        cur = local_var_initializer_sub(cur, var, mem->ty, &desg2);
        mem = mem->next;
      } while(mem && !peek_end() && consume(","));
    }

    if (has_open_brace && !consume_end()) {
      // "{" で始まってるのに、配列のサイズ個の初期化式を読んでもなおまだ初期化式が残っている場合、無視する
      skip_excess_elements();
    }

    // 残りのメンバーがまだあったらゼロ初期化する
    for (; mem; mem = mem->next) {
      Designator dseg2 = {desg, 0, mem};
      cur = lvar_init_zero(cur, var, mem->ty, &dseg2);
    }

    return cur;
  }

  // compound-literal で "{" 初期化式 "}" で初期化する場合に "{" が存在する
  // (int) { 1 } とか
  bool has_open_brace = consume("{");
  Node *e = assign();
  // fprintf(stdout, "val: %d\n", e->val);
  cur->next = new_desg_node(var, desg, e);
  if (has_open_brace) {
    expect_end();
  }
  return cur->next;
}

static Node *local_var_initializer(Var *var, Token *tk) {
  Node head = {};
  local_var_initializer_sub(&head, var, var->type, NULL);

  Node *node = new_node(ND_BLOCK, tk);
  node->body = head.next;

  return node;
}

static Node *var_decl() {
  Token *tk;
  if (is_type(token)) {
    StorageClass sclass = false;
    Type *type = basetype(&sclass);
    if (tk = consume(";")) {
      // basetypeの直後に;が来ていたらenumやstructの型の定義だけする場合
      // また、試したところ int; という意味ない文もエラーにはならないので、
      // 特に構造体やenumに限定せず任意のbasetypeの直後の ";" もいけそう。
      return new_node(ND_NULL, tk);
    }
    tk = token;
    char *ident_name;
    Type *basic_type = type;
    type = declarator(type, &ident_name);

    if (sclass == TYPEDEF) {
      expect(";");
      push_typedef_scope(ident_name, type);
      return new_node(ND_NULL, tk);
    }

    if (sclass == STATIC) {
      // static がついてたらスコープだけローカル変数で、グローバル変数扱いにする
      // ただ、単純にident_name でグローバル変数を作ってしまうと別の関数で、
      // 同じ名前のstatic local な変数を作れなくなる。
      //
      // なので、グローバル変数自体はユニークなラベルで名前をつけて、
      // スコープ側でその変数をここでパースしたident_nameで登録することで、同じ名前のstatic_local変数を別の関数でも使えるようにしている。
      Var *var = new_gvar(new_label(), type, true, true);
      // 別途スコープにident_nameでこの変数を登録
      push_var_scope(ident_name, var);

      if (!consume("=")) {
        if (type->is_incomplete) {
          error_at(tk->str, "incomplete element type(gvar)");
        }
      } else {
        var->initializer = gvar_initializer(type);
      }

      // カンマ区切りの別の宣言もあればパース
      static_var_decl_sub(basic_type, sclass);
      expect(";");
      return new_node(ND_NULL, tk);
    }

    Var *var = new_lvar(ident_name, type);
    Node *node = new_node(ND_VAR_DECL, tk);
    node->var = var;

    if (var->type->kind == TY_VOID) {
      error_at(tk->str, "void型の変数は宣言できません");
    }

    // 初期化式があるかどうかチェック
    if (consume("=")) {
      // 今作ったlocal変数に初期化式の値を代入するノードを設定
      node->initializer = local_var_initializer(var, tk);
    }

    if (type->is_incomplete) {
      error_at(tk->str, "incomplete element type(var_decl)");
    }

    // カンマ区切りの別の宣言もあればパース
    node = var_decl_sub(basic_type, node);
    expect(";");

    return node;
  }
  return NULL;
}

static void static_var_decl_sub(Type *base, StorageClass sclass) {
  while (consume(",")) {
    Token *tmp_tk = token;
    char *ident_name;
    Type *type = declarator(base, &ident_name);
    Var *var = new_gvar(new_label(), type, true, sclass == STATIC);
    push_var_scope(ident_name, var);

    if (!consume("=")) {
      if (type->is_incomplete) {
        error_at(tmp_tk->str, "incomplete element type(gvar)");
      }
      continue;
    }
    var->initializer = gvar_initializer(type);
  }
}

static Node *var_decl_sub(Type *base, Node *decl) {
  while (consume(",")) {
    Token *tk = token;
    char *ident_name;
    Type *type = declarator(base, &ident_name);

    Var *var = new_lvar(ident_name, type);
    Node *node = new_node(ND_VAR_DECL, tk);
    node->var = var;

    if (var->type->kind == TY_VOID) {
      error_at(tk->str, "void型の変数は宣言できません");
    }

    // 初期化式があるかどうかチェック
    if (tk = consume("=")) {
      // 今作ったlocal変数に初期化式の値を代入するノードを設定
      node->initializer = local_var_initializer(var, tk);
    }

    if (type->is_incomplete) {
      error_at(tk->str, "incomplete element type(var_decl_sub)");
    }

    decl = new_bin_node(ND_COMMA, decl, node, tk);
  }
  return decl;
}

static Node *expr() {
  Token *tk;
  Node *node = assign();

  while (tk = consume(",")) {
    // カンマがある場合、最後の式の値だけスタックに積みたいので、
    // カンマより左にある式の結果は全部式文(ND_EXPR_STMT)にして計算だけして結果は捨てる
    node = new_unary_node(ND_EXPR_STMT, node, node->tok);
    node = new_bin_node(ND_COMMA, node, assign(), tk);
  }

  return node;
}

static Node *assign() {
  Node *node = ternary();
  Token *tk;

  // chibicc だと "=" 以外の +=, -=... 計の演算子は別途 ND_ADD_EQ のような専用のNode種別をつくって処理している
  // https://github.com/rui314/chibicc/commit/05e907d2b8a94103d60148ce90e27ca191ad5446
  // 難しかったので、簡単そうな i += x を構文木上 i = i + x に読み替える方式にしてみる。
  // ただそのせいで、左辺のnodeが複数のnodeで共有されてしまってfree_nodesのときに複数回freeしてしまってエラーになったので、
  // chibccに合わせて今回からastのfreeはやめる
  if (tk = consume("=")) {
    return new_bin_node(ND_ASSIGN, node, assign(), tk);
  } else if (tk = consume("+=")) {
    Node *n = new_add_node(node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume("-=")) {
    Node *n = new_sub_node(node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume("*=")) {
    Node *n = new_bin_node(ND_MUL, node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume("/=")) {
    Node *n = new_bin_node(ND_DIV, node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume("<<=")) {
    Node *n = new_bin_node(ND_A_LSHIFT, node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume(">>=")) {
    Node *n = new_bin_node(ND_A_RSHIFT, node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume("|=")) {
    Node *n = new_bin_node(ND_BIT_OR, node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume("&=")) {
    Node *n = new_bin_node(ND_BIT_AND, node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  } else if (tk = consume("^=")) {
    Node *n = new_bin_node(ND_BIT_XOR, node, assign(), tk);
    return new_bin_node(ND_ASSIGN, node, n, tk);
  }
  return node;
}

static Node *ternary() {
  Token *tk;
  Node *node = logical_or();
  if (tk = consume("?")) {
    Node *true_expr = expr();
    expect(":");
    Node *false_expr = ternary();

    // ifと同じ構造で式になるのでthen, elsにstmtじゃなくてexprいれたら動いたので取り敢えずこれで
    Node *cond = node;
    node = new_node(ND_TERNARY, tk);
    node->cond = cond;
    node->then = true_expr;
    node->els = false_expr;

  }

  return node;
}

static long const_expr() {
  return eval(ternary());
}

static long eval2(Node *node, Var **var) {
  #pragma clang diagnostic ignored "-Wswitch"
  switch (node->kind) {
    case ND_ADD:
      return eval(node->lhs) + eval(node->rhs);
    case ND_PTR_ADD:
      return eval2(node->lhs, var) + eval(node->rhs);
    case ND_PTR_SUB:
      return eval2(node->lhs, var) - eval(node->rhs);
    case ND_SUB:
      return eval(node->lhs) - eval(node->rhs);
    case ND_MUL:
      return eval(node->lhs) * eval(node->rhs);
    case ND_DIV:
      return eval(node->lhs) / eval(node->rhs);
    case ND_BIT_AND:
      return eval(node->lhs) & eval(node->rhs);
    case ND_BIT_OR:
      return eval(node->lhs) | eval(node->rhs);
    case ND_BIT_XOR:
      return eval(node->lhs) ^ eval(node->rhs);
    case ND_A_LSHIFT:
      return eval(node->lhs) << eval(node->rhs);
    case ND_A_RSHIFT:
      return eval(node->lhs) >> eval(node->rhs);
    case ND_EQL:
      return eval(node->lhs) == eval(node->rhs);
    case ND_NOT_EQL:
      return eval(node->lhs) != eval(node->rhs);
    case ND_LT:
      return eval(node->lhs) < eval(node->rhs);
    case ND_LTE:
      return eval(node->lhs) <= eval(node->rhs);
    case ND_TERNARY:
      return eval(node->cond) ? eval(node->then) : eval(node->els);
    case ND_COMMA:
      return eval(node->rhs);
    case ND_NOT:
      return !eval(node->lhs);
    case ND_BIT_NOT:
      return ~eval(node->lhs);
    case ND_AND:
      return eval(node->lhs) && eval(node->rhs);
    case ND_OR:
      return eval(node->lhs) || eval(node->rhs);
    case ND_MOD:
      return eval(node->lhs) % eval(node->rhs);
    case ND_NUM:
      return node->val;
    case ND_ADDR:
      // ing gvar1[] = {1, 2}
      // int *gvar2 = &gvar1
      // のようなケースの2行目の&gvar1の処理
      // この場合、単にgvarの先頭アドレスが設定される
      //
      // 変数をついでに探さないモード(!var)か
      // 変数もついでに探すモード(*var) で、対象のノードが変数でなければエラー
      if (!var || *var || node->lhs->kind != ND_VAR) {
        error_at(token->str, "invalid addr initializer");
      }
      // 初期化式の中の変数をキャプチャする
      *var = node->lhs->var;
      return 0;
    case ND_VAR:
      // ing gvar1[] = {1, 2}
      // int *gvar2 = gvar1
      // のようなケースの2行目のgvar1の処理
      if (!var || *var || node->var->type->kind != TY_ARRAY) {
        error_at(token->str, "invalid var initializer");
      }
      *var = node->var;
      return 0;
  }
  #pragma clang diagnostic ignored "-Wreturn-type"
  error("定数式の計算のみ可能です");
}

static long eval(Node *node) {
  return eval2(node, NULL);
}


static Node *logical_or() {
  Token *tk;
  Node *node = logical_and();

  while (tk = consume("||")) {
    node = new_bin_node(ND_OR, node, logical_and(), tk);
  }

  return node;
}

static Node *logical_and() {
  Token *tk;
  Node *node = bit_or();

  while (tk = consume("&&")) {
    node = new_bin_node(ND_AND, node, bit_or(), tk);
  }

  return node;
}

static Node *bit_or() {
  Token *tk;
  Node *node = bit_xor();

  while (tk = consume("|")) {
    node = new_bin_node(ND_BIT_OR, node, bit_xor(), tk);
  }

  return node;
}

static Node *bit_xor() {
  Token *tk;
  Node *node = bit_and();

  while (tk = consume("^")) {
    node = new_bin_node(ND_BIT_XOR, node, bit_and(), tk);
  }

  return node;
}

static Node *bit_and() {
  Token *tk;
  Node *node = equality();

  while (tk = consume("&")) {
    node = new_bin_node(ND_BIT_AND, node, equality(), tk);
  }

  return node;
}

static Node *equality() {
  Token *tk;
  Node *node = relational();

  for (;;) {
    if (tk = consume("==")) {
      node = new_bin_node(ND_EQL, node, relational(), tk);
    } else if (tk = consume("!=")) {
      node = new_bin_node(ND_NOT_EQL, node, relational(), tk);
    } else {
      return node;
    }
  }
}

static Node *relational() {
  Token *tk;
  Node *node = shift();

  for (;;) {
    if (tk = consume("<=")) {
      node = new_bin_node(ND_LTE, node, shift(), tk);
    } else if (tk = consume(">=")) {
      node = new_bin_node(ND_LTE, shift(), node, tk);
    } else if (tk = consume("<")) {
      node = new_bin_node(ND_LT, node, shift(), tk);
    } else if (tk = consume(">")) {
      node = new_bin_node(ND_LT, shift(), node, tk);
    } else {
      return node;
    }
  }
}

static Node *shift() {
  Token *tk;
  Node *node = add();
  for (;;) {
    if (tk = consume("<<")) {
      node = new_bin_node(ND_A_LSHIFT, node, add(), tk);
    } else if (tk = consume(">>")) {
      node = new_bin_node(ND_A_RSHIFT, node, add(), tk);
    } else {
      return node;
    }
  }
}

static Node *new_add_node(Node *lhs, Node *rhs, Token *tk) {
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
    return new_bin_node(ND_ADD, lhs, rhs, tk);
  } else if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
    // ptr + num のケース
    return new_bin_node(ND_PTR_ADD, lhs, rhs, tk);
  } else if (is_integer(lhs->ty) && is_pointer(rhs->ty)){
    // num + ptrのケースなので、入れ替えてptr + num に正規化する
    return new_bin_node(ND_PTR_ADD, rhs, lhs, tk);
  }
  // ここに来た場合 ptr + ptr という不正なパターンになるので落とす
  error_at(token->str, "不正なaddパターンです。");
  return NULL;
}

static Node *new_sub_node(Node *lhs, Node *rhs, Token *tk) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
    // num - num
    return new_bin_node(ND_SUB, lhs, rhs, tk);
  } else if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
    //  ptr - ptr
    return new_bin_node(ND_PTR_DIFF, lhs, rhs, tk);
  } else if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
    // ptr - num
    return new_bin_node(ND_PTR_SUB, lhs, rhs, tk);
  }
  // ここに来た場合 num - ptr になってしまうので、エラーにする
  error("不正な演算です: num - ptr");
  return NULL; // error で落ちるので実際にはreturnされない
}

static Node *add() {
  Token *tk;
  Node *node = mul();

  for (;;) {
    if (tk = consume("+")) {
      node = new_add_node(node, mul(), tk);
    } else if (tk = consume("-")) {
      node = new_sub_node(node, mul(), tk);
    } else {
      return node;
    }
  }
}

static Node *mul() {
  Token *tk;
  Node *node = cast();

  for (;;) {
    if (tk = consume("*")) {
      node = new_bin_node(ND_MUL, node, cast(), tk);
    } else if (tk = consume("/")) {
      node = new_bin_node(ND_DIV, node, cast(), tk);
    } else if (tk = consume("%")) {
      node = new_bin_node(ND_MOD, node, cast(), tk);
    } else {
      return node;
    }
  }
}

static Node *cast() {
  Token *tk = token;
  if (consume("(")) {
    if (is_type(token)) {
      Type *ty = type_name();
      expect(")");
      if (!consume("{")) {
        // "{" が来た場合は、キャストではなくcompound-literaruの"(" typename ")" になるので、
        // この節はスキップして、token=tmpでもとに戻した上で、postfix側で処理する
        Node *node = new_unary_node(ND_CAST, cast(), tk);
        add_type(node->lhs);
        node->ty = ty;
        return node;
      }
    }
  }
  token = tk;
  Node *node = unary();
  add_type(node);

  return node;
}

static Node *unary() {
  Token *tk;
  if (consume("+")) {
    return cast();
  } else if (tk = consume("-")) {
    return new_bin_node(ND_SUB, new_num_node(0, tk), cast(), tk);
  } else if (tk = consume("&")) {
    return new_unary_node(ND_ADDR, cast(), tk);
  } else if (tk = consume("*")) {
    return new_unary_node(ND_DEREF, cast(), tk);
  } else if (tk = consume("++")) {
    return new_unary_node(ND_PRE_INC, cast(), tk);
  } else if (tk = consume("--")) {
    return new_unary_node(ND_PRE_DEC, cast(), tk);
  } else if (tk = consume("!")) {
    return new_unary_node(ND_NOT, cast(), tk);
  } else if (tk = consume("~")) {
    return new_unary_node(ND_BIT_NOT, cast(), tk);
  } else if (tk = consume_kind(TK_ALIGNOF)) {
    expect("(");
    Type *ty = type_name();
    expect(")");
    return new_num_node(ty->align, tk);
  } else if (consume_kind(TK_SIZEOF)) {
    Token *tmp = token;
    if (consume("(")) {
      if (is_type(token)) {
        Token *tk = token;
        Type *ty = type_name();

        // fprintf(stderr, "ty->kind:%d\n", ty->kind);
        // fprintf(stderr, "ty->isincomplete:%d\n", ty->is_incomplete);
        if (ty->is_incomplete) {
          error_at(tk->str, "incomplete element type(sizeof1)");
        }

        expect(")");
        return new_num_node(ty->size, tk);
      }
      // 型名じゃなかったらまた再度 "(" からパースやり直すためにconsume("(")の直前まで戻す
      token = tmp;
    }

    Token *tk = token;
    Node *n = cast();
    add_type(n);
    if (n->ty->is_incomplete) {
      error_at(tk->str, "incomplete element type(sizeof2)");
    }
    return new_num_node(node_type_size(n), tk);
  }
  return postfix();
}

Node *new_var_node(Var *var, Token *tk) {
  Node *node = new_node(ND_VAR, tk);
  node->var = var;
  add_type(node);
  return node;
}

static Node *parse_var(Token *t) {
  VarScope *sc = find_var(t);
  if (!sc) {
    error_at(t->str, "変数 %s は宣言されていません。",
             my_strndup(t->str, t->len));
  }
  if (sc->enum_ty) {
    return new_num_node(sc->enum_val, t);
  } else {
    return new_var_node(sc->var, t);
  }
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
    fprintf(stderr, "type_info: %s\n", type_info(lhs->ty));
    error_at(token->str, "not a struct(lhs->ty->kind: %d", lhs->ty->kind);
  }

  Token *tok = token;
  Member *m = find_member(lhs->ty, expect_ident());
  if (!m) {
    error_at(tok->str, "no such member");
  }

  Node *n = new_unary_node(ND_MEMBER, lhs, tok);
  n->member = m;

  return n;
}

static Node *postfix() {
  Token *tk;
  Node *node = compound_literal();
  if (node) {
    return node;
  }

  node = primary();
  for (;;) {
    if (tk = consume("[")) {
      node = new_bin_node(ND_DEREF, new_add_node(node, expr(), tk), NULL, tk);
      expect("]");
      continue;
    }

    if (consume(".")) {
      node = struct_ref(node);
      continue;
    }

    if (tk = consume("->")) {
      // x->y == (*x).y なので nodeをderefしたうえで . と同じ結果を返す
      node = new_unary_node(ND_DEREF, node, tk);
      node = struct_ref(node);
      continue;
    }

    if (tk = consume("++")) {
      node = new_unary_node(ND_POST_INC, node, tk);
      continue;
    }

    if (tk = consume("--")) {
      node = new_unary_node(ND_POST_DEC, node, tk);
      continue;
    }

    break;
  }
  return node;
}

static Node *compound_literal() {
  Token *tmp = token;
  if (!consume("(") || !is_type(token)) {
    token = tmp;
    return NULL;
  }
  Type *ty = type_name();
  expect(")");

  if(!peek_token("{")) {
    token = tmp;
    return NULL;
  }

  tmp = token;
  if (scope_depth == 0) {
    // 無名のグローバル変数追加
    Var *var = new_gvar(new_label(), ty, true, false);
    var->initializer = gvar_initializer(ty);
    return new_var_node(var, tmp);
  } else {
    // 無名のローカル変数追加
    Var *var = new_lvar(new_label(), ty);
    Node *node = new_var_node(var, tmp);
    node->init = local_var_initializer(var, tmp);
    return node;
  }

}

static Node *parse_call_func(Token *t) {
  Node *node = new_node(ND_CALL, t);
  node->funcname = my_strndup(t->str, t->len);

  VarScope *func_var_sc = find_var(t);
  if (func_var_sc) {
    Var *func_var = func_var_sc->var;
    if (func_var->type->kind != TY_FUNC) {
      // 関数名でなかったらエラー
      error_at(t->str, "%s is not a function name", node->funcname);
    }
    // 関数の戻り値の型をND_CALLの戻り値にする
    node->ty = func_var->type->return_ty;
  } else if (!strcmp(node->funcname, "__builtin_va_start")) {
    // var_start 用の組込み関数かをチェック
    node->ty = void_type;
  } else {
    fprintf(stderr, "関数: %s は宣言されていません\n", node->funcname);
    // 未宣言の関数を呼び出す場合は警告だけだして一旦int型を返す関数として扱う
    node->ty = int_type;
  }

  if (consume(")")) {
    // 閉じカッコがきたら引数なし関数
    add_type(node);
    return node;
  }

  // 引数パース
  // 引数を評価するときは最後の引数から評価したいので、node->argには逆順のリストを設定する
  int args = 0;
  Node *cur = assign();
  args++;
  while (consume(",")) {
    Node *arg = assign();
    arg->next = cur;
    cur = arg;
    args++;
  }
  // f(1, 2, 3)という関数呼び出しのnode->argは
  // 3 -> 2 -> 1
  // になる
  node->arg = cur;

  node->funcarg_num = args;

  expect(")");

  add_type(node);

  return node;
}

static char *new_label() {
  static int label_index = 0;
  char buf[100];
  int n = sprintf(buf, ".L.data.%03d", label_index++);
  return my_strndup(buf, n);
}

static Node *parse_string_literal(Token *str_token) {
  Type *ty = array_of(char_type, str_token->content_length);
  Var *var = new_gvar(new_label(), ty, true, true);
  var->initializer = new_init_string(str_token->contents, str_token->content_length);

  return new_var_node(var, str_token);
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
  t = token;
  return new_num_node(expect_number(), t);
}

