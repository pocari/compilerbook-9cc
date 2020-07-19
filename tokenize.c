#include "ynicc.h"

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
    case TK_INT: // int
      return "TK_INT";
    case TK_CHAR: // char
      return "TK_CHAR";
    case TK_SIZEOF: // sizeof
      return "TK_SIZEOF";
    case TK_STR: // 文字列リテラル
      return "TK_STR";
    case TK_EOF: // 入力終了
      return "TK_EOF";
  }
  error("不正なtokenです");

  return NULL;
}

void dump_token(Token *t) {
  printf("## (Token kind: %*s ", 12 ,token_kind_to_s(t->kind));

  switch (t->kind) {
    case TK_NUM:
      printf("[%d]", t->val);
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
    "int",
    TK_INT,
  },
  {
    "char",
    TK_CHAR,
  },
  {
    "sizeof",
    TK_SIZEOF,
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

// 文字列リテラルをトークナイズした結果を返す
// また、トークナイズした結果でポインタもすすめる
static Token *tokenize_string_literal(Token *current_token, char **current_pointer) {
  char *p = *current_pointer;

  char *s = p++;
  while (*p && (*p != '"')) {
    p++;
  }
  if (!(*p)) {
    error_at(s, "文字列が終了していません。");
  }
  // *p がここで閉じる " を指してるので一つすすめる
  p++;

  Token *tok = new_token(TK_STR, current_token, s, p - s);
  // この時点でs, p はそれぞれ
  //   "abc"
  //   ^    ^
  //   |    |
  //   s    p
  // を指しているので、文字列として確保する部分は
  // "a"の部分(s + 1) から、 "c"までの長さ((p - s) - 2))
  // をstrndupで個別に確保(abcの部分)
  //
  tok->contents = my_strndup(s + 1, (p - s) - 2);
  // \0まで含めた長さ
  tok->content_length = (p - s) - 1;

  *current_pointer = p;

  return tok;
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

    // fprintf(stderr, "c ... %c\n", *p);
    Keyword *key = tokenize_keyword(p);
    if (key) {
      int keyword_len = strlen(key->keyword);
      cur = new_token(key->kind, cur, p, keyword_len);
      p += keyword_len;
      continue;
    }

    if (starts_with(p, "<=") || starts_with(p, ">=") || starts_with(p, "==") || starts_with(p, "!=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    if (is_ident_first_char(*p)) {
      // fprintf(stderr, "tokenize ident(%c)\n", *p);
      char *s = p; // 識別子の先頭
      while (is_alnum(*p)) {
        p++;
      }
      cur = new_token(TK_IDENT, cur, s, p - s); //p - s で文字列長さになる
      continue;
    }

    if (*p == '"') {
      cur = tokenize_string_literal(cur, &p);
      // // fprintf(stderr, "tokenize string\n");
      // char *s = p++;
      // while (*p && *p++ != '"') {
      //   // noop
      // }
      // if (!(*p)) {
      //   error_at(s, "文字列が終了していません。");
      // }
      // cur = new_token(TK_STR, cur, s, p - s);
      // // この時点でs, p はそれぞれ
      // //   "abc"
      // //   ^    ^
      // //   |    |
      // //   s    p
      // // を指しているので、文字列として確保する部分は
      // // "a"の部分(s + 1) から、 "c"までの長さ((p - s) - 2))
      // // をstrndupで個別に確保(abcの部分)
      // //
      // cur->contents = my_strndup(s + 1, (p - s) - 2);
      // // \0まで含めた長さ
      // cur->content_length = (p - s) - 1;
      continue;
    }

    if (ispunct(*p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      cur->val = strtol(p, &p, 10);
      continue;
    }


    error_at(p, "トークナイズできません");
  }
  new_token(TK_EOF, cur, p, 0);

  return head.next;
}

