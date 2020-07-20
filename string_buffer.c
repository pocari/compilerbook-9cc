#include "ynicc.h"

#define INITIAL_BUF_SIZE 16

typedef struct StringBuffer {
  char *buf;
  int str_len;
  int buf_len;
} StringBuffer;

static void expand_buffer(StringBuffer *sb) {
  assert(sb);
  sb->buf_len *= 2;
  sb->buf = realloc(sb->buf, sb->buf_len);
}

StringBuffer *sb_init() {
  StringBuffer *sb = calloc(1, sizeof(StringBuffer));

  sb->str_len = 0;
  sb->buf_len = INITIAL_BUF_SIZE;
  sb->buf = calloc(INITIAL_BUF_SIZE, sizeof(char));

  return sb;
}

void sb_append_char(StringBuffer *sb, char ch) {
  if (sb->str_len >= sb->buf_len) {
    expand_buffer(sb);
  }
  sb->buf[sb->str_len++] = ch;
}

void sb_free(StringBuffer *sb) {
  assert(sb);

  free(sb->buf);
  free(sb);
}

char *sb_str(StringBuffer *sb) {
  return sb->buf;
}

int sb_str_len(StringBuffer *sb) {
  return sb->str_len;
}

