void *calloc();
int free();
int memcpy();
int printf();

// 各局面がすでに生成されたかどうかのフラグ
char *seen;

typedef struct State {
  struct State *parent;
  int board[3][3];
} State;

typedef struct Queue {
  struct Queue *head;
  struct Queue *tail;
  struct Queue *next;
  State *val;
} Queue;

Queue *init_queue() {
  Queue *q = calloc(1, sizeof(Queue));
  q->head = q->tail = q->next = 0;
  return q;
}

void free_queue(Queue *queue) {
  free(queue);
}

State *dequeue(Queue *q) {
  if (!q->head) {
    return 0;
  }

  State *v = q->head->val;
  Queue *tmp = q->head;
  q->head = q->head->next;
  free_queue(tmp);

  return v;
}

void enqueue(Queue *q, State *v) {
  Queue *entry = init_queue();
  entry->val = v;

  if (q->tail) {
    q->tail->next = entry;
    q->tail = entry;
  } else {
    q->tail = entry;
  }

  if (!q->head) {
    q->head = entry;
  }
}

void cleanup_queue(Queue *q) {
  while (q->head) {
    dequeue(q);
  }
  free_queue(q);
}

int hash_value(State *s) {
  //左上から順番に並べた数字をhash値とする
  //ので、最小(0)12345678 〜 876543210
  //になる。
  //char型だと約836MBぐらいなのでムダも多いが
  //hash値をseenのindexに対応させて1, 0で生成済みかどうかのフラグにするj
  int h = 0;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      h = h * 10 + s->board[i][j];
    }
  }
  return h;

}

int goal_hash() {
  return 12345678;
}

void dump_board(State *s) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (s->board[i][j] == 0) {
        printf("  ");
      } else {
        printf("%2d", s->board[i][j]);
      }
    }
    printf("\n");
  }
  printf("\n");
}

int search_free_pos(int board[][3], int *r, int *c) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (board[i][j] == 0) {
        *r = i;
        *c = j;
        return 0; // まだ値なしreturnができないので適当に返す
      }
    }
  }
  return 0;
}

int can_move(int row, int col) {
  if ((0 <= row && row <= 2) && (0 <= col && col <= 2)) {
    return 1;
  }
  return 0;
}

State *new_state(State *parent, int zero_r, int zero_c, int new_r, int new_c) {
  State *s = calloc(1, sizeof(State));

  s->parent = parent;
  memcpy(s->board, parent->board, sizeof(parent->board));

  s->board[zero_r][zero_c] = s->board[new_r][new_c];
  s->board[new_r][new_c] = 0;

  return s;
}

State *solve_helper(Queue *q, State *initial_state) {
  enqueue(q, initial_state);
  State *s;

  // 移動する方向
  int direction[4][2] = {
    { 0,  1}, // 右
    { 0, -1}, // 左
    {-1,  0}, // 上
    { 1,  0} // 下
  };

    // printf("fetch\n");
  while (s = dequeue(q)) {
    int hash = hash_value(s);
    if (hash == goal_hash()) {
      return s;
    } else {
      if (seen[hash]) {
        continue;
      }
      seen[hash] = 1;

      int r, c;
      search_free_pos(s->board, &r, &c);
      // printf("free(%d, %d)\n", r, c);
      // dump_board(s);
      for (int i = 0; i < 4; i++) {
        int row = r + direction[i][0];
        int col = c + direction[i][1];
        if (can_move(row, col)) {
          State *ns = new_state(s, r, c, row, col);
          if (!seen[hash_value(ns)]) {
            // printf("move %d(%d, %d)\n", i, row, col);
            // dump_board(ns);
            enqueue(q, ns);
          }
        }
      }
    }
  }

  return 0;
}

int dump_step(State *s) {
  if (s) {
    int step = dump_step(s->parent);
    if (step == 0) {
      printf("initial state:\n");
    } else {
      printf("step %d:\n", step);
    }
    dump_board(s);
    return step + 1;
  }
  return 0;
}

int solve(State *initial_state) {
  Queue *q = init_queue();
  State *result = solve_helper(q, initial_state);
  if (result) {
    dump_step(result);
    printf("solved\n");
  } else {
    printf("not solved\n");
  }
  cleanup_queue(q);
  return 0;
}

int main() {
  seen = calloc(876543210 + 1, sizeof(char));
  // 構造体の初期化がまだできないので、配列2初期状態を設定してコピーする
  int initial_board[3][3] = {
    // 31手
    // {8, 0, 6},
    // {5, 4, 7},
    // {2, 3, 1}

    // 5手
    {3, 1, 2},
    {4, 7, 0},
    {6, 8, 5}
  };

  State s;
  s.parent = 0;
  memcpy(s.board, initial_board, sizeof(initial_board));

  solve(&s);

  return 0;
}

