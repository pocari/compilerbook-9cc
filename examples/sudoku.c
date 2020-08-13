int printf();

void dump_board(int (*board)[9]) {
  int i;
  int j;

  for (i = 0; i < 9; i = i + 1) {
    if (i != 0) {
      if (i % 3 == 0) {
        printf(" ------+-------+------\n");
      }
    }
    for (j = 0; j < 9; j = j + 1) {
      if (j != 0) {
        if (j % 3 == 0) {
          printf(" |");
        }
      }
      if (board[i][j]) {
        printf("%2d", board[i][j]);
      } else {
        printf(" .");
      }

    }
    printf("\n");
  }
  printf("\n");
}

int free_pos_count(int (*board)[9]) {
  int i;
  int j;

  int count = 0;
  for (i = 0; i < 9; i = i + 1) {
    for (j = 0; j < 9; j = j + 1) {
      if (board[i][j] == 0) {
        count = count + 1;
      }
    }
  }
  return count;
}

int pos_to_row(int pos) {
  return pos / 9;
}

int pos_to_col(int pos) {
  return pos % 9;
}

int next_free_pos(int (*board)[9], int pos) {
  int i;
  for (i = pos; i < 9 * 9; i = i + 1) {
    int r = pos_to_row(i);
    int c = pos_to_col(i);
    if (board[r][c] == 0) {
      return i;
    }
  }
  return -1;
}

int can_put(int (*board)[9], int row, int col, int num) {
  int i;

  //縦のチェック
  for (i = 0; i < 9; i = i + 1) {
    if (board[i][col] == num) {
      // すでに同じ列にnumがあったらおけない
      return 0;
    }
  }
  //横のチェック
  for (i = 0; i < 9; i = i + 1) {
    if (board[row][i] == num) {
      // すでに同じ行にnumがあったらおけない
      return 0;
    }
  }

  // 同じブロックのチェック
  int r_offset = row / 3;
  int c_offset = col / 3;
  int r;
  int c;

  for (r = 0; r < 3; r = r + 1) {
    for (c = 0; c < 3; c = c + 1) {
      if (board[r + 3 * r_offset][c + 3 * c_offset] == num) {
        //すでに同じブロックにnumがあったらおけない
        return 0;
      }
    }
  }

  return 1;
}

int solve(int (*board)[9], int pos) {
  // printf("(%d, %d) free: %d\n", row, col, free_pos_count());
  if (free_pos_count(board) == 0) {
    return 1;
  } else {
    int free_pos = next_free_pos(board, pos);
    if (free_pos != -1) {
      int n;
      for (n = 1; n <= 9; n = n + 1) {
        int r = pos_to_row(free_pos);
        int c = pos_to_col(free_pos);
        int ok = can_put(board, r, c, n);
        // printf("(%d, %d) ... (%d, %s)\n", r, c, n, (ok ? "ok" : "not ok"));
        if (ok) {
          board[r][c] = n;
          if (solve(board, free_pos + 1)) {
            return 1;
          }
          board[r][c] = 0;
        }
      }
    }
  }
  return 0;
}


int main() {
  int board[9][9] = {
    {0, 0, 0, 0, 7, 0, 0, 0, 0},
    {0, 3, 0, 0, 0, 0, 0, 4, 0},
    {0, 0, 0, 1, 0, 0, 0, 8, 0},

    {1, 0, 0, 0, 0, 0, 0, 0, 6},
    {0, 0, 9, 3, 0, 0, 0, 0, 5},
    {0, 8, 0, 2, 0, 9, 0, 0, 0},

    {0, 0, 0, 6, 0, 0, 0, 0, 2},
    {0, 0, 0, 4, 1, 0, 0, 3, 0},
    {0, 4, 2, 0, 0, 8, 7, 9, 0}
  };

  dump_board(board);
  if (solve(board, 0)) {
    dump_board(board);
  } else {
    printf("not solved\n");
  }

  return 0;
}
