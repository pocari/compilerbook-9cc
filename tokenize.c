#include "9cc.h"

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
  case TK_RESERVED: // RESERVEDとなっているが今の時点では + または - の記号
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

    if (strncmp(p, "return", 6) == 0 && !is_alnum(p[6])) {
      cur = new_token(TK_RETURN, cur, p, 6);
      p += 6;
      continue;
    }

    if (strncmp(p, "while", 5) == 0 && !is_alnum(p[5])) {
      cur = new_token(TK_WHILE, cur, p, 5);
      p += 5;
      continue;
    }

    if (starts_with(p, "<=") || starts_with(p, ">=") || starts_with(p, "==") || starts_with(p, "!=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
    }

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')' || *p == '>' || *p == '<' || *p == ';' || *p == '=' || *p == '{' || *p == '}') {
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

