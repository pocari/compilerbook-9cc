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

};

typedef struct Scope Scope;
struct Scope {
  VarScope *var_scope;
  TagScope *tag_scope;
};

typedef enum {
  TYPEDEF = 1 << 0,
  STATIC  = 1 << 1,
} StorageClass;

// 現在パース中のスコープにある変数(ローカル、グローバル含む)を管理する
static VarScope *var_scope;

// 現在パース中のスコープにある構造体のタグ名を管理する
static TagScope *tag_scope;

// 今パース中の関数のローカル変数(+仮引数)
static VarList *locals = NULL;

// グローバル変数
static VarList *globals = NULL;

static Scope *enter_scope(void) {
  Scope *new_scope = calloc(1, sizeof(Scope));

  new_scope->var_scope = var_scope;
  new_scope->tag_scope = tag_scope;

  return new_scope;
}

static void leave_scope(Scope *prev_scope) {
  var_scope = prev_scope->var_scope;
  tag_scope = prev_scope->tag_scope;
}

static void push_tag_scope(Token *tag_tok, Type *type) {
  TagScope *sc = calloc(1, sizeof(TagScope));

  sc->name = my_strndup(tag_tok->str, tag_tok->len);
  sc->ty = type;
  sc->next = tag_scope;

  tag_scope = sc;
}

static VarScope *push_var_scope_helper(char *name) {
  VarScope *sc = calloc(1, sizeof(VarScope));

  sc->name = name;
  sc->next = var_scope;
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

static Type *find_tag(Token *tk) {
  for (TagScope *sc = tag_scope; sc; sc = sc->next) {
    if (strncmp(sc->name, tk->str, tk->len) == 0) {
      return sc->ty;
    }
  }
  return NULL;
}

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

static Node *new_num_node(long val) {
  Node *node = new_node(ND_NUM);
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

static Var *new_gvar(char *name, Type *type, bool emit) {
  Var *var = new_var(name, type, false);

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
  // 型の途中でtypedef/staticが出てくる場合もあるのでtypedef/static自体も型名の一つとしてtrueにしておき
  // 詳細はbasetype関数で判断する
  switch (tk->kind) {
    case TK_TYPEDEF:
    case TK_STRUCT:
    case TK_ENUM:
    case TK_STATIC:
      return true;
    default:
      return find_typedef(tk);
  }
}

// ynicc BNF
//
// program                   = (
//                               func_decls
//                             | global_var_decls
//                           )*
// basetype                  = ("void" | "_Bool" | "int" | "char" | "long" | "long" "long" | "short" | struct_decl | typedef_types | enum_decl)
// declarator                =  "*" * ( "(" declarator         ")" | ident )  type_suffix
// abstract_declarator       =  "*" * ( "(" abstract_declarator")"         )? type_suffix
// typename                  = basetype abstract_declarator type_suffix
// type_suffix               = ("[" number "]" type_suffix)?
// function_def_or_decl      = basetype declarator "(" function_params? ")" ("{" stmt* "}" | ";")
// global_var_decls          = basetype declarator ";"
// stmt                      = expr ";"
//                           | "{" stmt* "}"
//                           | "return" expr ";"
//                           | "if" "(" expr ")" stmt ("else" stmt)?
//                           | "while" "(" expr ")" stmt
//                           | "for" "(" (expr | var_decl)? ";" expr? ";" expr? ")" stmt
//                           | var_decl
// var_decl                  = basetype var_decl_sub ("," var_decl_sub)* ";"
// var_decl_sub              = declarator ( "=" local_var_initializer)?
// local_var_initializer     = local_var_initializer_sub
// local_var_initializer_sub = "{" local_var_initializer_sub ("," local_var_initializer_sub)* "}"
//                           | assign
// struct_decl               = "struct" ident
//                           | "struct" ident? "{" struct_member* "}"
// struct_member             = basetype declarator
// 
// 以降の演算子の優先順位は下記参照(この表で上にある演算子(優先度が高い演算子)ほどBNFとしては下にくる)
// http://www.bohyoh.com/CandCPP/C/operator.html
// expr                      = assign ("," assign)*
// assign                    = logical_or ( ("="  | "+=" | "-=" | "*=" | "/=") assign)?
// logical_or                = logical_and ( "||" logical_and )*
// logical_and               = bit_or ( "&&" bit_or )*
// bit_or                    = bit_and ( "|" bit_and )*
// bit_xor                   = bit_and ( "&" bit_and )*
// bit_and                   = equality ( "^" equality )*
// equality                  = relational ("==" relational | "!=" relational)*
// relational                = add ("<" add | "<=" add | ">" add | ">=" add)*
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
//                           | postfix
// postfix                   = parimary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
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
static Node *expr();
static Node *assign();
static Node *bit_and();
static Node *logical_or();
static Node *logical_and();
static Node *bit_or();
static Node *bit_xor();
static Node *equality();
static Node *relational();
static Node *add();
static Node *new_add_node(Node *lhs, Node *rhs);
static Node *new_sub_node(Node *lhs, Node *rhs);
static Node *mul();
static Node *cast();
static Node *unary();
static Node *postfix();
static Node *primary();
static Node *read_expr_stmt();
static Type *type_suffix(Type *base);
static Type *struct_decl();
static Type *enum_decl();

typedef enum {
  NEXT_DECL_DUMMY,
  NEXT_DECL_FUNCTION_DEF,
  NEXT_DECL_GLOBAL_VAR_OR_TYPEDEF,
} next_decl_type;

static next_decl_type next_decl() {
  // 先読みした結果今の位置に戻る用に今のtokenを保存
  Token *tmp = token;
  bool is_func, is_global_var;

  StorageClass sclass = 0;
  Type *ty = basetype(&sclass);

  if (sclass == TYPEDEF) {
    // typedefがあったら、それは関数ではないとみなす
    token = tmp;
    return NEXT_DECL_GLOBAL_VAR_OR_TYPEDEF;
  }

  char *name;
  declarator(ty, &name);
  is_func = consume("(");

  // 先読みした結果、関数定義かグローバル変数かわかったので、tokenを元に戻す
  token = tmp;

  if (is_func) {
    // カッコがあれば関数定義
    return NEXT_DECL_FUNCTION_DEF;
  }

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

// グローバル変数のパース
static void parse_gvar() {
  StorageClass sclass = false;
  Type *type = basetype(&sclass);
  char *ident_name;
  type = declarator(type, &ident_name);
  expect(";");

  if (sclass == TYPEDEF) {
    push_typedef_scope(ident_name, type);
    return;
  }

  new_gvar(ident_name, type, true);
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
        parse_gvar();
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

    if (peek_kind(TK_TYPEDEF) || peek_kind(TK_STATIC)) {
      if (!sclass) {
        error_at(tk->str, "ストレージクラス指定子はここでは使えません");
      }
      // 一度でもどこかにtypedef/staticがついていたら覚えておく
      if (consume_kind(TK_TYPEDEF)) {
        *sclass |= TYPEDEF;
      } else if (consume_kind(TK_STATIC)) {
        *sclass |= STATIC;
      }

      // typedef と static を同時に使っていたらエラーにする
      if ((*sclass & TYPEDEF) && (*sclass & STATIC)) {
        error_at(tk->str, "staticとtypedefは同時に指定できません");
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

  return ty;;
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
  type = declarator(type, &ident);
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

  // struct ident の後に {がなければ 既存の構造体の変数定義になるので、
  // tkをタグ名として構造体の定義を探して返す
  if (tk) {
    if (!peek_token("{")) {
      Type *st = find_tag(tk);
      if (!st) {
        error_at(bak->str, "構造体: %s が見つかりません", my_strndup(tk->str, tk->len));
      }
      if (st->kind != TY_STRUCT) {
        error_at(bak->str, "%s は構造体ではありません", my_strndup(tk->str, tk->len));
      }
      return st;
    }
  }

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

  // 構造体のタグが存在していたら、今パースした構造体の定義をタグ名をキーに登録
  if (tk) {
    push_tag_scope(tk, type);
  }

  return type;
}

static Type *enum_decl() {
  expect_token(TK_ENUM);

  Token *bak = token;

  Token *tag_tk = consume_ident();

  if (tag_tk) {
    if (!peek_token("{")) {
      // 構造体と同じくenum tagの次に"{"がない定義済みenum型の変数宣言になるので、tag名で探す
      Type *t = find_tag(tag_tk);
      if (!t) {
        error_at(bak->str, "enum: %s が見つかりません", my_strndup(tag_tk->str, tag_tk->len));
      }
      if (t->kind != TY_ENUM) {
        error_at(bak->str, "%s はenumではありません", my_strndup(tag_tk->str, tag_tk->len));
      }
      return t;
    }
  }
  // ここに来た場合は新規のenumの定義
  expect("{");
  Type *ty = enum_type();
  int enum_value = 0;
  for (;;) {
    char *ident = expect_ident();
    if (consume("=")) {
      enum_value = expect_number();
    }
    push_enum_scope(ident, ty, enum_value++);

    if (consume("}")) {
      break;
    }

    expect(",");

    if (consume("}")) {
      break;
    }
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

  Type *type = basetype(NULL);
  char *name;
  type = declarator(type, &name);
  Var *var = new_lvar(name, type);
  VarList *var_list = calloc(1, sizeof(VarList));
  var_list->var = var;
  // fprintf(stderr, "parse func param start\n");
  while (!consume(")")) {
    expect(",");
    type = basetype(NULL);
    char *name;
    type = declarator(type, &name);
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
  int offset = 0;
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
  new_gvar(func->name, func_type(func->return_type), false);

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
  Node *node = new_node(ND_EXPR_STMT);
  node->kind = ND_EXPR_STMT;
  node->lhs = expr();
  return node;
}

static Node *stmt() {
  Node *node = NULL;

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
  } else if (consume("{")) {
    node = new_node(ND_BLOCK);
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
static Type *type_suffix(Type *base) {
  if (!consume("[")) {
    return base;
  }
  int array_size = expect_number();
  expect("]");
  base = type_suffix(base);
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

  node = new_unary_node(ND_DEREF, node);
  add_type(node);

  return node;
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

  Node *e = assign();
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
  if (is_type(token)) {
    StorageClass sclass = false;
    Type *type = basetype(&sclass);
    Token *tmp_tk = token;
    char *ident_name;
    Type *basic_type = type;
    type = declarator(type, &ident_name);

    if (sclass == TYPEDEF) {
      expect(";");
      push_typedef_scope(ident_name, type);
      return new_node(ND_NULL);
    }

    Var *var = new_lvar(ident_name, type);
    Node *node = new_node(ND_VAR_DECL);
    node->var = var;

    if (var->type->kind == TY_VOID) {
      error_at(tmp_tk->str, "void型の変数は宣言できません");
    }

    // 初期化式があるかどうかチェック
    if (consume("=")) {
      // 今作ったlocal変数に初期化式の値を代入するノードを設定
      node->initializer = local_var_initializer(var);
    }
    // カンマ区切りの別の宣言もあればパース
    node = var_decl_sub(basic_type, node);
    expect(";");

    return node;
  }
  return NULL;
}

static Node *var_decl_sub(Type *base, Node *decl) {
  while (consume(",")) {
    Token *tmp_tk = token;
    char *ident_name;
    Type *type = declarator(base, &ident_name);

    Var *var = new_lvar(ident_name, type);
    Node *node = new_node(ND_VAR_DECL);
    node->var = var;

    if (var->type->kind == TY_VOID) {
      error_at(tmp_tk->str, "void型の変数は宣言できません");
    }

    // 初期化式があるかどうかチェック
    if (consume("=")) {
      // 今作ったlocal変数に初期化式の値を代入するノードを設定
      node->initializer = local_var_initializer(var);
    }
    decl = new_bin_node(ND_COMMA, decl, node);
  }
  return decl;
}

static Node *expr() {
  Node *node = assign();

  while (consume(",")) {
    // カンマがある場合、最後の式の値だけスタックに積みたいので、
    // カンマより左にある式の結果は全部式文(ND_EXPR_STMT)にして計算だけして結果は捨てる
    node = new_unary_node(ND_EXPR_STMT, node);
    node = new_bin_node(ND_COMMA, node, assign());
  }

  return node;
}

static Node *assign() {
  Node *node = logical_or();

  // chibicc だと "=" 以外の +=, -=... 計の演算子は別途 ND_ADD_EQ のような専用のNode種別をつくって処理している
  // https://github.com/rui314/chibicc/commit/05e907d2b8a94103d60148ce90e27ca191ad5446
  // 難しかったので、簡単そうな i += x を構文木上 i = i + x に読み替える方式にしてみる。
  // ただそのせいで、左辺のnodeが複数のnodeで共有されてしまってfree_nodesのときに複数回freeしてしまってエラーになったので、
  // chibccに合わせて今回からastのfreeはやめる
  if (consume("=")) {
    return new_bin_node(ND_ASSIGN, node, assign());
  } else if (consume("+=")) {
    Node *n = new_add_node(node, assign());
    return new_bin_node(ND_ASSIGN, node, n);
  } else if (consume("-=")) {
    Node *n = new_sub_node(node, assign());
    return new_bin_node(ND_ASSIGN, node, n);
  } else if (consume("*=")) {
    Node *n = new_bin_node(ND_MUL, node, assign());
    return new_bin_node(ND_ASSIGN, node, n);
  } else if (consume("/=")) {
    Node *n = new_bin_node(ND_DIV, node, assign());
    return new_bin_node(ND_ASSIGN, node, n);
  }
  return node;
}

static Node *logical_or() {
  Node *node = logical_and();

  while (consume("||")) {
    node = new_bin_node(ND_OR, node, logical_and());
  }

  return node;
}

static Node *logical_and() {
  Node *node = bit_or();

  while (consume("&&")) {
    node = new_bin_node(ND_AND, node, bit_or());
  }

  return node;
}

static Node *bit_or() {
  Node *node = bit_xor();

  while (consume("|")) {
    node = new_bin_node(ND_BIT_OR, node, bit_xor());
  }

  return node;
}

static Node *bit_xor() {
  Node *node = bit_and();

  while (consume("^")) {
    node = new_bin_node(ND_BIT_XOR, node, bit_and());
  }

  return node;
}

static Node *bit_and() {
  Node *node = equality();

  while (consume("&")) {
    node = new_bin_node(ND_BIT_AND, node, equality());
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
  Node *node = cast();

  for (;;) {
    if (consume("*")) {
      node = new_bin_node(ND_MUL, node, cast());
    } else if (consume("/")) {
      node = new_bin_node(ND_DIV, node, cast());
    } else if (consume("%")) {
      node = new_bin_node(ND_MOD, node, cast());
    } else {
      return node;
    }
  }
}

static Node *cast() {
  Token *tmp = token;
  if (consume("(")) {
    if (is_type(token)) {
      Type *ty = type_name();
      expect(")");
      Node *node = new_unary_node(ND_CAST, cast());
      add_type(node->lhs);
      node->ty = ty;
      return node;
    }
  }
  token = tmp;
  Node *node = unary();
  add_type(node);

  return node;
}

static Node *unary() {
  if (consume("+")) {
    return cast();
  } else if (consume("-")) {
    return new_bin_node(ND_SUB, new_num_node(0), cast());
  } else if (consume("&")) {
    return new_unary_node(ND_ADDR, cast());
  } else if (consume("*")) {
    return new_unary_node(ND_DEREF, cast());
  } else if (consume("++")) {
    return new_unary_node(ND_PRE_INC, cast());
  } else if (consume("--")) {
    return new_unary_node(ND_PRE_DEC, cast());
  } else if (consume("!")) {
    return new_unary_node(ND_NOT, cast());
  } else if (consume("~")) {
    return new_unary_node(ND_BIT_NOT, cast());
  } else if (consume_kind(TK_SIZEOF)) {
    Token *tmp = token;
    if (consume("(")) {
      if (is_type(token)) {
        Type *ty = type_name();
        expect(")");
        return new_num_node(ty->size);
      }
      // 型名じゃなかったらまた再度 "(" からパースやり直すためにconsume("(")の直前まで戻す
      token = tmp;
    }

    Node *n = cast();
    add_type(n);
    return new_num_node(node_type_size(n));
  }
  return postfix();
}

Node *new_var_node(Var *var) {
  Node *node = new_node(ND_VAR);
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
    return new_num_node(sc->enum_val);
  } else {
    return new_var_node(sc->var);
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
    add_type(node);
    if (consume("[")) {
      node = new_bin_node(ND_DEREF, new_add_node(node, expr()), NULL);
      expect("]");
      continue;
    }

    if (consume(".")) {
      node = struct_ref(node);
      continue;
    }

    if (consume("->")) {
      // x->y == (*x).y なので nodeをderefしたうえで . と同じ結果を返す
      node = new_unary_node(ND_DEREF, node);
      node = struct_ref(node);
      continue;
    }

    if (consume("++")) {
      node = new_unary_node(ND_POST_INC, node);
      continue;
    }

    if (consume("--")) {
      node = new_unary_node(ND_POST_DEC, node);
      continue;
    }

    break;
  }
  return node;
}

static Node *parse_call_func(Token *t) {
  Node *node = new_node(ND_CALL);
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
  int args = 0;
  Node *cur = node->arg = assign();
  args++;
  while (consume(",")) {
    cur->next = assign();
    cur = cur->next;
    args++;
  }
  node->funcarg_num = args;

  if (args > 6) {
    fprintf(stderr, "関数呼び出しの引数が6を超えると今はコンパイル出来ません\n");
  }
  expect(")");

  add_type(node);

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
  Var *var = new_gvar(next_data_label(), ty, true);
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

