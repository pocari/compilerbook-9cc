typedef struct hoge {
  int x;
  int y;
} hoge;
typedef hoge *hoge_p;

int main() {
  hoge m;
  hoge_p n = &m;

  n->x = 1;
  n->y = 2;

  printf("m.x ... %d\n", m.x);
  printf("m.y ... %d\n", m.y);

  return 0;
}

