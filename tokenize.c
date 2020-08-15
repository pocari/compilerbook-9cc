#include "ynicc.h"
#include <stdio.h>
#include <string.h>

Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

char *token_kind_to_s(TokenKind kind) {
  switch (kind) {
    case TK_DUMMY: // ダミー
      return "TK_DUMMY";
    case TK_RESERVED: // 記号系
      return "TK_RESERVED";
    case TK_IDENT: // 識別子
      return "TK_IDENT";
    case TK_NUM: // 整数トークン
      return "TK_NUM";
    case TK_RETURN: // returnトークン
      return "TK_RETURN";
    case TK_WHILE: // while
      return "TK_WHILE";
    case TK_IF: // if
      return "TK_IF";
    case TK_ELSE: // else
      return "TK_ELSE";
    case TK_FOR: // for
      return "TK_FOR";
    case TK_VOID: // void
      return "TK_VOID";
    case TK_BOOL: // bool
      return "TK_BOOl";
    case TK_INT: // int
      return "TK_INT";
    case TK_CHAR: // char
      return "TK_CHAR";
    case TK_LONG: // long
      return "TK_LONG";
    case TK_SHORT: // short
      return "TK_SHORT";
    case TK_STRUCT: // struct
      return "TK_STRUCT";
    case TK_ENUM: // enum
      return "TK_ENUM";
    case TK_SIZEOF: // sizeof
      return "TK_SIZEOF";
    case TK_STR: // 文字列リテラル
      return "TK_STR";
    case TK_TYPEDEF: // typedef
      return "TK_TYPEDEF";
    case TK_STATIC: // static
      return "TK_STATIC";
    case TK_EOF: // 入力終了
      return "TK_EOF";
  }
  error("不正なtokenです");

  return NULL;
}

void dump_token(Token *t) {
  if (!t) {
    printf("## (NULL token)");
    return;
  }

  printf("## (Token kind: %*s ", 12 ,token_kind_to_s(t->kind));

  switch (t->kind) {
    case TK_NUM:
      printf("[%ld]", t->val);
      break;
    case TK_STR:
      printf("[contents: [%s], length(includes \\0): %d", t->contents, t->content_length);
      break;
    default:
      printf("[%s]", my_strndup(t->str, t->len));
      break;
  }
  printf(")\n");
}

void free_tokens(Token *cur) {
  while (cur) {
    Token *tmp = cur->next;
    free(cur);
    cur  = tmp;
  }
}

bool is_alnum(char c) {
  return
    ('a' <= c && c <= 'z') ||
    ('A' <= c && c <= 'Z') ||
    ('0' <= c && c <= '9') ||
    (c == '_');
}

bool is_ident_first_char(char c) {
  return
    ('a' <= c && c <= 'z') ||
    ('A' <= c && c <= 'Z') ||
    (c == '_');
}

bool starts_with(char *p, char *str) {
  return memcmp(p, str, strlen(str)) == 0;
}

typedef struct Keyword {
  char *keyword;
  TokenKind kind;
} Keyword;

static Keyword keywords[] = {
  {
    "return",
    TK_RETURN,
  },
  {
    "while",
    TK_WHILE,
  },
  {
    "if",
    TK_IF,
  },
  {
    "else",
    TK_ELSE,
  },
  {
    "for",
    TK_FOR,
  },
  {
    "void",
    TK_VOID,
  },
  {
    "int",
    TK_INT,
  },
  {
    "char",
    TK_CHAR,
  },
  {
    "long",
    TK_LONG,
  },
  {
    "short",
    TK_SHORT,
  },
  {
    "_Bool",
    TK_BOOL,
  },
  {
    "struct",
    TK_STRUCT,
  },
  {
    "enum",
    TK_ENUM,
  },
  {
    "sizeof",
    TK_SIZEOF,
  },
  {
    "typedef",
    TK_TYPEDEF,
  },
  {
    "static",
    TK_STATIC,
  },
};

Keyword *tokenize_keyword(char *p) {
  for (int i = 0; i < sizeof(keywords) / sizeof(Keyword); i++) {
    Keyword *k = &keywords[i];
    int len = strlen(k->keyword);
    if (strncmp(p, k->keyword, len) == 0 && !is_alnum(p[len])) {
      return k;
    }
  }
  return NULL;
};

static int parse_escaped(int ch) {
  switch(ch) {
    case '0':
      return 0;
    case 'n':
      return '\n';
    case 't':
      return '\t';
    case '\\':
      return '\\';
    case '"':
      return '"';
    case '\'':
      return '\'';
    default:
      return ch;
  }
}

// 文字リテラルをトークナイズした結果を返す
// また、トークナイズした結果でポインタもすすめる
static Token *tokenize_char_literal(Token *current_token, char **current_pointer) {
  int val;
  char *p = *current_pointer;

  // この関数に入ってきた時点でpは開き"'"を指しているので、中身を見るために一つすすめる
  // sは開始位置の保存用
  char *s = p++;

  if (*p == '\\') {
    p++;
    val = parse_escaped(*p);
  } else {
    val = *p;
  }
  char *tmp = p++;
  if (!(*p)) {
    error_at(tmp, "文字が終了していません。");
  }

  // ここでpが閉じる側の"'"を指していなければエラー
  if (*p != '\'') {
    error_at(p, "\"'\" が閉じられていません。");
  }

  // この時点でpが"'"を指しているので、その次の位置でcurrent_pointerを更新
  p++;
  *current_pointer = p;

  Token *tk = new_token(TK_NUM, current_token, s, 0);
  tk->val = val;

  return tk;
}

// 文字列リテラルをトークナイズした結果を返す
// また、トークナイズした結果でポインタもすすめる
static Token *tokenize_string_literal(Token *current_token, char **current_pointer) {
  string_buffer *sb = sb_init();
  char *p = *current_pointer;

  char *s = p++;
  while (*p && (*p != '"')) {
    if (*p == '\\') {
      p++;
      sb_append_char(sb, parse_escaped(*p));
    } else {
      sb_append_char(sb, *p);
    }
    p++;
  }
  if (!(*p)) {
    error_at(s, "文字列が終了していません。");
  }
  // *p がここで閉じる " を指してるので一つすすめる
  p++;

  Token *tok = new_token(TK_STR, current_token, s, p - s);
  tok->contents = my_strndup(sb_str(sb), sb_str_len(sb));
  // \0まで含めた長さ
  tok->content_length = sb_str_len(sb) + 1;

  sb_free(sb);

  *current_pointer = p;

  return tok;
}

// p が8進数の数値の開始かどうかを返す
static bool starts_with_octal(char *p) {
  // 前提として*pは何らかの数値というところまでわかっている
  if (*p++ != '0') {
    // 先頭が0出ない場合は8進数でない
    return false;
  }
  if (isdigit(*p)) {
    // 先頭の0につづいてまだなにかの数字があれば8進数とみなす
    return true;
  }
  return false;
}

static bool is_hex_digit(char c) {
  return
    ('a' <= c && c <= 'f') ||
    ('A' <= c && c <= 'F') ||
    ('0' <= c && c <= '9');
}

static bool is_octal_digit(char c) {
  return '0' <= c && c <= '7';
}

static bool is_binary_digit(char c) {
  return '0' <= c && c <= '1';
}

static int hex_val(char c) {
  if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  } else if ('A' <= c && c <= 'F') {
    return c - 'A' + 10;
  }
  return c - '0';
}

static int octal_val(char c) {
  return c - '0';
}

static int binary_val(char c) {
  return c - '0';
}

static Token *tokenize_hex_number(Token *cur, char **cur_pos) {
  long val = 0;
  char *p = *cur_pos;

  // "0x" (2文字分)を読み飛ばす
  p += 2;
  while (p && is_hex_digit(*p)) {
    val = (val << 4) + hex_val(*p);
    p++;
  }

  if (!p) {
    error_at(p, "数値が不正です");
  }

  Token *tok = new_token(TK_NUM, cur, *cur_pos, 0);
  tok->val = val;

  *cur_pos = p;

  return tok;
}

static Token *tokenize_octal_number(Token *cur, char **cur_pos) {
  long val = 0;
  char *p = *cur_pos;

  // "0" (1文字分)を読み飛ばす
  p += 1;
  while (p && is_octal_digit(*p)) {
    val = (val << 3) + octal_val(*p);
    p++;
  }

  if (!p) {
    error_at(p, "数値が不正です");
  }

  Token *tok = new_token(TK_NUM, cur, *cur_pos, 0);
  tok->val = val;

  *cur_pos = p;

  return tok;
}

static Token *tokenize_binary_number(Token *cur, char **cur_pos) {
  long val = 0;
  char *p = *cur_pos;

  // "0b" (2文字分)を読み飛ばす
  p += 2;
  while (p && is_binary_digit(*p)) {
    val = (val << 1) + binary_val(*p);
    p++;
  }

  if (!p) {
    error_at(p, "数値が不正です");
  }

  Token *tok = new_token(TK_NUM, cur, *cur_pos, 0);
  tok->val = val;

  *cur_pos = p;

  return tok;
}
static Token *tokenize_number(Token *cur, char **cur_pos) {
  Token *tok = NULL;

  if (starts_with(*cur_pos, "0x")) {
    // 16進数
    tok = tokenize_hex_number(cur, cur_pos);
  } else if (starts_with(*cur_pos, "0b")) {
    // 2進数
    tok = tokenize_binary_number(cur, cur_pos);
  } else if (starts_with_octal(*cur_pos)) {
    // 8進数
    tok = tokenize_octal_number(cur, cur_pos);
  } else {
    // 10進数
    tok = new_token(TK_NUM, cur, *cur_pos, 0);
    tok->val = strtol(*cur_pos, cur_pos, 10);
  }

  return tok;
}

static char *multi_char_ope[]= {
  "<=",
  ">=",
  "==",
  "!=",
  "->",
  "++",
  "--",
  "+=",
  "-=",
  "*=",
  "/=",
  "&&",
  "||",
  NULL,
};

// 複数文字からなるオペレータにマッチするかを返す。
// マッチする場合そのオペレータの文字数、マッチしない場合は0を返す
int is_multi_char_operator(char *p) {
  for (char **op = multi_char_ope; *op; op++) {
    if (starts_with(p, *op)) {
      return strlen(*op);
    }
  }
  return 0;
}

Token *tokenize(char *p) {
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // fprintf(stderr, "*p ... %c\n", *p);
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (starts_with(p, "//")) {
      p += 2;
      while (*p != '\n') {
        p++;
      }
      continue;
    }

    if (starts_with(p, "/*")) {
      char *q = strstr(p + 2, "*/");
      if (!q) {
        error_at(p, "ブロックコメントが閉じられていません");
      }
      p = q + 2;
      continue;
    }

    // fprintf(stderr, "c ... %c\n", *p);
    Keyword *key = tokenize_keyword(p);
    if (key) {
      int keyword_len = strlen(key->keyword);
      cur = new_token(key->kind, cur, p, keyword_len);
      assert(cur);
      p += keyword_len;
      continue;
    }

    int len = is_multi_char_operator(p);
    if (len) {
      cur = new_token(TK_RESERVED, cur, p, len);
      assert(cur);
      p += len;
      continue;
    }

    if (is_ident_first_char(*p)) {
      // fprintf(stderr, "tokenize ident(%c)\n", *p);
      char *s = p; // 識別子の先頭
      while (is_alnum(*p)) {
        p++;
      }

      cur = new_token(TK_IDENT, cur, s, p - s); //p - s で文字列長さになる
      assert(cur);
      continue;
    }

    if (*p == '"') {
      cur = tokenize_string_literal(cur, &p);
      assert(cur);
      continue;
    }
    if (*p == '\'') {
      cur = tokenize_char_literal(cur, &p);
      assert(cur);
      continue;
    }


    if (ispunct(*p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      assert(cur);
      continue;
    }

    if (isdigit(*p)) {
      cur = tokenize_number(cur, &p);
      assert(cur);
      continue;
    }


    error_at(p, "トークナイズできません");
  }
  new_token(TK_EOF, cur, p, 0);

  return head.next;
}

char *read_file(char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    error("cannot open %s: %s", path, strerror(errno));
  }

  // ファイルの最後の位置を調べてサイズ計算
  if (fseek(fp, 0, SEEK_END) == -1) {
    error("%s: fseek: %s", path, strerror(errno));
  }
  size_t file_size = ftell(fp);

  // 読むために先頭に戻す
  if (fseek(fp, 0, SEEK_SET) == -1) {
    error("%s: fseek: %s", path, strerror(errno));
  }

  // \n\0で終わらせるための2バイト追加で確保
  char *buf = calloc(1, file_size + 2);
  fread(buf, file_size, 1, fp);


  if (file_size == 0 || buf[file_size - 1] != '\n') {
    buf[file_size++] = '\n';
  }
  buf[file_size] = '\0';
  fclose(fp);

  return buf;
}

