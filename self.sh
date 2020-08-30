#!/bin/bash -x
set -e

TMP=tmp-self

rm -rf $TMP
mkdir -p $TMP

expand() {
    file=$1
    cat <<EOF > $TMP/$1
typedef struct FILE FILE;
extern FILE *stdout;
extern FILE *stderr;

int isdigit(int c);
int ispunct(int c);
int putchar(int c);
int fseek(FILE *fp, long offset, int origin);
long int ftell(FILE *stream);
int fclose(FILE *stream);
int memcmp(void *buf1, void *buf2,long n);
void *malloc(long size);
void *realloc(void *ptr, long size);
void *calloc(long nmemb, long size);
int *__errno_location();
char *strerror(int errnum);
FILE *fopen(char *pathname, char *mode);
long fread(void *ptr, long size, long nmemb, FILE *stream);
int feof(FILE *stream);
static void assert() {}
int strcmp(char *s1, char *s2);
int printf(char *fmt, ...);
int fprintf(FILE *out, char *fmt, ...);
int sprintf(char *buf, char *fmt, ...);
long strlen(char *p);
int strncmp(char *p, char *q);
void *memcpy(char *dst, char *src, long n);
char *strndup(char *p, long n);
int isspace(int c);
char *strstr(char *haystack, char *needle);
long strtol(char *nptr, char **endptr, int base);
void free(void* p);
typedef struct {
  int gp_offset;
  int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} __va_elem;

typedef __va_elem va_list[1];

int vfprintf(FILE *stderr, char *fmt, va_list ap);
int vprintf(char *fmt, va_list ap);
static void va_start(__va_elem *ap) {
  __builtin_va_start(ap);
}
void exit(void);
static void va_end(__va_elem *ap) {}
EOF
    grep -v '^ *#' ynicc.h >> $TMP/$1
    grep -v '^ *#' $1 >> $TMP/$1
    sed -i 's/\bbool\b/_Bool/g' $TMP/$1
    sed -i 's/\berrno\b/*__errno_location()/g' $TMP/$1
    sed -i 's/\btrue\b/1/g; s/\bfalse\b/0/g;' $TMP/$1
    sed -i 's/\bNULL\b/0/g' $TMP/$1
    sed -i 's/INT_MAX/2147483647/g' $TMP/$1
    sed -i 's/SEEK_END/2/g' $TMP/$1
    sed -i 's/SEEK_SET/0/g' $TMP/$1

    ./ynicc $TMP/$1 > $TMP/${1%.c}.s
    gcc -c -o $TMP/${1%.c}.o $TMP/${1%.c}.s
}

cp *.c $TMP
# for i in $TMP/*.c; do
#   gcc -I. -c -o ${i%.c}.o $i
# done

expand ynicc.c
expand parser.c
expand codegen.c
expand string_buffer.c
expand tokenize.c
expand debug.c
expand type.c

gcc -static -o ynicc-gen2 $TMP/*.o
