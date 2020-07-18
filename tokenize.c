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
    case TK_EOF: // 入力終了
      return "TK_EOF";
  }
  error("不正なtokenです");

  return NULL;
}

void dump_token(Token *t) {
  printf("## (Token kind: %*s [%s])\n", 12 ,token_kind_to_s(t->kind), my_strndup(t->str, t->len));
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

    if (is_ident_first_char(*p)) {
      char *s = p; // 識別子の先頭
      while (is_alnum(*p)) {
        p++;
      }
      cur = new_token(TK_IDENT, cur, s, p - s); //p - s で文字列長さになる
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

