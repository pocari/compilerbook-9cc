#include "9cc.h"
#include <ctype.h>

Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

void dump_token(Token *t) {
  char *kind_str;
  switch (t->kind) {
  case TK_RESERVED: // 記号系
    kind_str = "TK_RESERVED";
    break;
  case TK_IDENT: // 識別子
    kind_str = "TK_IDENT";
    break;
  case TK_NUM: // 整数トークン
    kind_str = "TK_NUM";
    break;
  case TK_RETURN: // returnトークン
    kind_str = "TK_RETURN";
    break;
  case TK_WHILE: // while
    kind_str = "TK_WHILE";
    break;
  case TK_IF: // if
    kind_str = "TK_IF";
    break;
  case TK_ELSE: // else
    kind_str = "TK_ELSE";
    break;
  case TK_FOR: // for
    kind_str = "TK_FOR";
    break;
  case TK_EOF: // 入力終了
    kind_str = "TK_EOF";
    break;
  default:
    error("不正なtokenです");
  }
  fprintf(stderr, "(Token kind: %s)\n", kind_str);
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

    if (ispunct(*p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      cur->val = strtol(p, &p, 10);
      continue;
    }

    // とりあえず連続したアルファベット小文字を識別子とみなす
    if ('a' <= *p && *p <= 'z') {
      char *s = p; // 識別子の先頭
      while ('a' <= *p && *p <= 'z') {
        p++;
      }
      cur = new_token(TK_IDENT, cur, s, p - s); //p - s で文字列長さになる
      continue;
    }

    error_at(p, "トークナイズできません");
  }
  new_token(TK_EOF, cur, p, 0);

  return head.next;
}

