#include "ynicc.h"

#define INITIAL_BUF_SIZE 16

struct string_buffer {
  char *buf;
  int str_len;
  int buf_len;
};

static void expand_buffer(string_buffer *sb) {
  assert(sb);
  sb->buf_len *= 2;
  sb->buf = realloc(sb->buf, sb->buf_len);
}

static void ensure_buffer(string_buffer *sb) {
  if (sb->str_len >= sb->buf_len) {
    expand_buffer(sb);
  }
}

string_buffer *sb_init() {
  string_buffer *sb = calloc(1, sizeof(string_buffer));

  sb->str_len = 0;
  sb->buf_len = INITIAL_BUF_SIZE;
  sb->buf = calloc(INITIAL_BUF_SIZE, sizeof(char));

  return sb;
}

void sb_append_char(string_buffer *sb, int ch) {
  ensure_buffer(sb);
  sb->buf[sb->str_len++] = ch;
}

void sb_free(string_buffer *sb) {
  assert(sb);

  free(sb->buf);
  free(sb);
}

char *sb_str(string_buffer *sb) {
  return sb->buf;
}

int sb_str_len(string_buffer *sb) {
  return sb->str_len;
}

