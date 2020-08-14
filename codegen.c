#include "ynicc.h"
#include <assert.h>

// 今コード生成厨の関数名
static char *funcname;

static void gen(Node *node);

static int printfln(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vprintf(fmt, ap);
  putchar('\n');

  return n + 1;
}

//スタックの先頭に積まれている値をアドレスとみなして、引数の型のサイズに合わせた値をそのアドレスから取得してスタックに積む
static void load(Type *t) {
  printfln("  pop rax"); // スタックにつまれている、変数のアドレスをraxにロード
  // 変数のアドレスにある値をraxにロード
  int sz = t->size;
  if (sz == 1) {
    printfln("  movsx rax, BYTE PTR [rax]");
  } else if (sz == 2) {
    printfln("  movsx rax, WORD PTR [rax]");
  } else if (sz == 4) {
    printfln("  movsxd rax, DWORD PTR [rax]");
  } else if (sz == 8) {
    printfln("  mov rax, [rax]");
  } else {
    assert(false);
  }

  printfln("  push rax"); // 変数の値(rax)をスタックに積む
}

// スタックの先頭を値、 スタックの2番目の値をアドレスとみなして、そのアドレスに値を設定して、 値をスタックに積む
static void store(Type *t) {
  printfln("  pop rdi"); // rhsの結果
  printfln("  pop rax"); // 左辺の変数のアドレス

  if (t->kind == TY_BOOL) {
    // booleanの場合はrdiが
    // 0のときは0
    // それ以外は固定で1をrdiに設定する
    printfln("  cmp rdi, 0");
    // cmp の比較で rdi != 0 のときだけ rdiの下位8bit に1をセット
    printfln("  setne dil");
    // rdiの上位56bitはクリアして1を代入
    printfln("  movzb rdi, dil");
  }

  int sz = t->size;
  // 左辺の変数にrhsの結果を代入
  if (sz == 1) {
    printfln("  mov [rax], dil");
  } else if (sz == 2) {
    printfln("  mov [rax], di");
  } else if (sz == 4) {
    printfln("  mov [rax], edi");
  } else if (sz == 8) {
    printfln("  mov [rax], rdi");
  } else {
    assert(false);
  }
  printfln("  push rdi"); // この代入結果自体もスタックに積む(右結合でどんどん左に伝搬していくときの右辺値になる)
}

static void gen_addr(Node *node) {
  // switchの警告を消すpragma
  #pragma clang diagnostic ignored "-Wswitch"
  switch(node->kind) {
    case ND_VAR:
      printfln("  # gen_addr start (var_name: %s, var_type: %s)", node->var->name, node->var->is_local ? "local" : "global");
      printfln("  # gen_addr-ND_VAR start");
      if (node->var->is_local) {
        printfln("  mov rax, rbp");
        printfln("  sub rax, %d", node->var->offset);
        printfln("  push rax");
      } else {
        // global変数の場合は単にそのラベル(=変数名)をpushする
        printfln("  push offset %s", node->var->name);
      }
      printfln("  # gen_addr-ND_VAR end");
      printfln("  # gen_addr end");
      return;
    case ND_DEREF:
      printfln("  # gen_addr start (var_name: none)");
      printfln("  # gen_addr-ND_DEREF start");
      gen(node->lhs);
      printfln("  # gen_addr-ND_DEREF stop");
      printfln("  # gen_addr end");
      return;
    case ND_MEMBER:
      printfln("  # gen_addr start (struct-member: %s)", node->member->name);
      printfln("  # gen_addr-ND_MEMBER start");
      printfln("  # struct var ref start");
      gen_addr(node->lhs);
      printfln("  # struct var ref end");
      printfln("  # struct member ref start");
      printfln("  pop rax"); // 構造体変数のアドレス
      printfln("  add rax, %d", node->member->offset); //構造体のアドレスを元にそのメンバーの位置(オフセットを設定)
      printfln("  push rax"); // 構造体メンバーのアドレスをpush
      printfln("  # struct member ref end");
      printfln("  # gen_addr-ND_MEMBER stop");
      printfln("  # gen_addr end");
      return;
  }

 error("代入の左辺値が変数ではありません");
}

// スタックの先頭の値をインクリメントしてスタックに積み直す
static void inc(Type *ty) {
  printfln("  pop rax");
  // ポインタの場合はそのポインタが指す先の型のサイズ分インクリメントする。(int *x; x++ が4バイト先に進むような場合)
  // ポインタでない場合は単に値を1増やす int i = 0; i++; でiが1になる みたいな場合
  printfln("  add rax, %ld\n", ty->ptr_to ? ty->ptr_to->size : 1);
  printfln("  push rax");
}

// スタックの先頭の値をデクリメントしてスタックに積み直す
static void dec(Type *ty) {
  printfln("  pop rax");
  printfln("  sub rax, %ld\n", ty->ptr_to ? ty->ptr_to->size : 1);
  printfln("  push rax");
}

static int next_label_key() {
  static int label_index = 0;
  return label_index++;
}

static void cast(Node *node) {
  printfln(" pop rax");

  if (node->ty->kind == TY_BOOL) {
    printfln("  cmp rax, 0");
    printfln("  setne al");
  }

  if (node->ty->size == 1) {
    printfln("  movsx rax, al");
  } else if (node->ty->size == 2) {
    printfln("  movsx rax, ax");
  } else if (node->ty->size == 4) {
    printfln("  movsxd rax, eax");
  }

  printfln(" push rax");
}

// 引数に渡す時用のレジスタ
// ND_CALLの実装参照
static char *ARGUMENT_REGISTERS_SIZE8[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9",
};

static char *ARGUMENT_REGISTERS_SIZE4[] = {
    "edi", "esi", "edx", "ecx", "r8d", "r9d",
};

static char *ARGUMENT_REGISTERS_SIZE2[] = {
    "di", "si", "dx", "cx", "r8w", "r9w",
};

static char *ARGUMENT_REGISTERS_SIZE1[] = {
    "dil", "sil", "dl", "cl", "r8b", "r9b",
};

static void gen(Node *node) {
  assert(node);
  // switchの警告を消すpragma
  #pragma clang diagnostic ignored "-Wswitch"
  switch (node->kind) {
    case ND_NUM:
      // 32bitを超える整数を直接pushできないので、32bit以内は直接
      // 超える場合は一度レジスタ(64bit)経由でpushする
      //
      // 実際にためしたところ、リテラルに書けるのは符号付き32bit整数のようなので、
      // 正の数の場合
      //   push 2147483647  (== 2^31 - 1)
      // はOKで、
      //   push 2147483648  (== 2^31 )
      // だと、 Error: operand type mismatch for `push' のエラーになる
      if (node->val == (int)node->val) {
        // intのサイズに収まる場合は普通にpushする
        printfln("  push %ld", node->val);
      } else {
        // intを超える数値リテラルの場合はレジスタ経由でpush
        // movabsのabsはよくわからなかった
        printfln("  movabs rax, %ld", node->val);
        printfln("  push rax");
      }
      return;
    case ND_MEMBER:
    case ND_VAR:
      if (node->kind == ND_VAR) {
        if (node->var) {
          printfln("  # ND_VAR start(var_name: %s)", node->var->name);
        } else {
          printfln("  # ND_VAR start(var_name: none)");
        }
      } else {
        assert(node->kind == ND_MEMBER);
        // .  の ND_MEMBER の場合はそのまま node->lhs->var->nameでいいが
        // -> の ND_MEMBER の場合は、node->lhsが構造体へのポインタになってるのでそれも考慮する
        if (node->lhs->kind == ND_DEREF) {
          // -> の場合は、node->lhs->lhsが構造体への変数
          printfln("  # ND_VAR start(struct_var_name: %s, member_name: %s)", node->lhs->lhs->var->name, node->member->name);
        } else {
          // . の場合は、node->lhsが構造体の変数
          printfln("  # ND_VAR start(struct_var_name: %s, member_name: %s)", node->lhs->var->name, node->member->name);
        }
      }
      gen_addr(node);
      if (node->ty->kind != TY_ARRAY) {
        // 配列以外の場合は、識別子が指すアドレスにある値をスタックにつむところまでやるが、だが、
        // 配列の場合は、識別子が指すアドレス自体をスタックにつみたいので、そうする。
        load(node->ty);
      }
      printfln("  # ND_VAR end");
      return;
    case ND_ASSIGN:
      printfln("  # ND_ASSIGN start");
      gen_addr(node->lhs);
      gen(node->rhs);
      //rhsの結果がスタックの先頭、その次に変数のアドレスが入ってるのでそれをロード
      store(node->ty);
      printfln("  # ND_ASSIGN end");
      return;
    case ND_RETURN:
      printfln("  # ND_RETURN start");
      gen(node->lhs);
      printfln("  pop rax");
      printfln("  jmp .L.return.%s", funcname);
      printfln("  # ND_RETURN end");
      return;
    case ND_BLOCK:
      printfln("  # ND_BLOCK start");
      // このブロックの先頭の文からコード生成
      //
      for (Node *n = node->body; n; n = n->next) {
        gen(n);
      }
      printfln("  # ND_BLOCK end");
      return;
    case ND_IF:
      {
        printfln("  # ND_IF start");
        gen(node->cond); // 条件式のコード生成
        if (node->els) {
          // else ありの if
          printfln("  pop rax"); // 条件式の結果をraxにロード
          printfln("  cmp rax, 0"); // 条件式の結果チェック
          int else_label = next_label_key();
          int end_label = next_label_key();
          printfln("  je .L.else.%04d", else_label); // false(rax == 0)ならwhile終了なのでジャンプ
          gen(node->then);                         // true節のコード生成
          printfln("  jmp .L.end.%04d", end_label); // true節のコードが終わったのでif文抜ける
          printfln(".L.else.%04d:", else_label); // elseのときの飛崎
          gen(node->els);                      // false節のコード生成
          printfln(".L.end.%04d:", end_label); // elseのときの飛崎
        } else {
          // else なしの if
          printfln("  pop rax"); // 条件式の結果をraxにロード
          printfln("  cmp rax, 0"); // 条件式の結果チェック
          int end_label = next_label_key();
          printfln("  je .L.end.%04d", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
          gen(node->then);                       // true節のコード生成
          printfln(".L.end.%04d:", end_label); // elseのときの飛び先
        }
        printfln("  # ND_IF end");
      }
      return;
    case ND_WHILE:
      {
        printfln("  # ND_WHILE start");
        int begin_label = next_label_key();
        printfln(".L.begin.%04d:", begin_label);
        printfln("  # ND_WHILE condition start");
        gen(node->cond); // 条件式のコード生成
        printfln("  # ND_WHILE condition end");
        printfln("  pop rax"); // 条件式の結果をraxにロード
        printfln("  cmp rax, 0"); // 条件式の結果チェック
        int end_label = next_label_key();
        printfln("  je .L.end.%04d", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
        printfln("  # ND_WHILE body start");
        gen(node->body); // whileの本体実行
        printfln("  # ND_WHILE body end");
        printfln("  jmp .L.begin.%04d", begin_label); //繰り返し
        printfln(".L.end.%04d:", end_label);
        printfln("  # ND_WHILE end");
      }
      return;
    case ND_FOR:
      {
        printfln("  # ND_FOR start");
        int begin_label = next_label_key();
        int end_label = next_label_key();
        // 初期化式
        if (node->init) {
          gen(node->init);
        }
        printfln(".L.begin.%04d:", begin_label);
        // 条件式
        if (node->cond) {
          gen(node->cond);
          printfln("  pop rax"); // 条件式の結果をraxにロード
          printfln("  cmp rax, 0"); // 条件式の結果チェック
          printfln("  je .L.end.%04d", end_label); // false(rax == 0)ならwhile終了なのでジャンプ
        }
        // for の中身のアセンブラ
        gen(node->body);
        // 継続式
        if (node->inc) {
          gen(node->inc);
        }
        printfln("  jmp .L.begin.%04d", begin_label); //繰り返し
        printfln(".L.end.%04d:", end_label);

        printfln("  # ND_FOR end");
      }
      return;
    case ND_CALL:
      {
        // 引数を渡すときにつかうレジスタは
        // https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf
        // の
        // Figure 3.4: Register Usage
        // を参照(引数1から引数6までは rdi, rsi, rdx, rcx, r8, r9の順に積む)
        printfln("  # ND_CALL start");
        if (node->funcarg_num > 0) {
          Node *cur = node->arg;
          for (int i = 0; i < node->funcarg_num; i++) {
            // 引数のアセンブリを出力(どんどん引数の式の値がスタックに積まれる)
            printfln("  # func call argument %d", i + 1);
            gen(cur);
            cur = cur->next;
          }

          // スタックから引数用のレジスタに値をロード
          for (int i = node->funcarg_num - 1; i >= 0; i--) {
            printfln("  # load argument %d to register", i + 1);
            printfln("  pop %s", ARGUMENT_REGISTERS_SIZE8[i]);
          }
        }
        // printfを呼ぶときは、 al レジスタに浮動小数点の可変長引数の数をalレジスタに入れておく必要があるが
        // 現状は浮動小数点が無いので、固定で0をいれておく
        printfln("  xor al, al");
        printfln("  call %s", node->funcname);
        printfln("  push rax"); // 関数の戻り値をスタックに積む
        printfln("  # ND_CALL end");
      }
      return;
    case ND_ADDR:
      printfln("  # ND_ADDR start");
      gen_addr(node->lhs);
      printfln("  # ND_ADDR end");
      return;
    case ND_DEREF:
      printfln("  # ND_DEREF start");
      gen(node->lhs);
      if (node->ty->kind != TY_ARRAY) {
        // 配列以外の場合は、識別子が指すアドレスにある値(がアドレスなので)それをスタックにつむところまでやるが、だが、
        // 配列の場合は、識別子が指すアドレス自体をスタックにつみたいので、そうする。
        load(node->ty);
      }
      printfln("  # ND_DEREF end");
      return;
    case ND_VAR_DECL:
      if (node->initializer) {
        gen(node->initializer);
      }
      return;
    case ND_EXPR_STMT:
      gen(node->lhs);
      // 式文なので、結果を捨てる
      printfln("  add rsp, 8");
      return;
    case ND_CAST:
      gen(node->lhs);
      cast(node);
      return;
    case ND_COMMA:
      gen(node->lhs);
      gen(node->rhs);
      return;
    case ND_PRE_INC:
      gen_addr(node->lhs);
      printfln("  push [rsp]");
      load(node->ty);
      inc(node->ty);
      store(node->ty);
      return;
    case ND_PRE_DEC:
      gen_addr(node->lhs);
      printfln("  push [rsp]");
      load(node->ty);
      dec(node->ty);
      store(node->ty);
      return;
    case ND_POST_INC:
      gen_addr(node->lhs);
      printfln("  push [rsp]");
      load(node->ty);
      inc(node->ty);
      store(node->ty);
      // x = i++;
      // のようなケースでは、 変数iの値自体はインクリメントするが、
      // xに設定される値としては、インクリメント前の値なのでスタックの値だけデクリメントしてもとに戻す
      dec(node->ty);
      return;
    case ND_POST_DEC:
      gen_addr(node->lhs);
      printfln("  push [rsp]");
      load(node->ty);
      dec(node->ty);
      store(node->ty);
      inc(node->ty);
      return;
    case ND_NULL:
      // typedef でパース時のみ発生し具体的なコード生成が無いノード
      printfln("  # ND_NULL ");
      return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printfln("  pop rdi");
  printfln("  pop rax");

  switch (node->kind) {
    case ND_ADD:
      printfln("  add rax, rdi");
      break;
    case ND_PTR_ADD:
      printfln("  imul rdi, %d", node->ty->ptr_to->size);
      printfln("  add rax, rdi");
      break;
    case ND_SUB:
      printfln("  sub rax, rdi");
      break;
    case ND_PTR_SUB:
      printfln("  imul rdi, %d", node->ty->ptr_to->size);
      printfln("  sub rax, rdi");
      break;
    case ND_PTR_DIFF:
      // 普通に引き算した結果をポインタの指す先の型のサイズで割って、個数に変換
      // 例)
      // int ary[] = { ... };
      // int *third = ary + 3;
      // int z = third - ary;
      // z が 3になるようにする。
      //
      // rax - rdi
      // の結果が
      // rax
      // に入るので、
      // raxをlhsのポインターが指す型のサイズで割った商をraxに入れる。
      // このため、割る数(rdi)に「ポインターが指す方のサイズ」を設定する必要がある
      printfln("  sub rax, rdi");
      printfln("  cqo");
      printfln("  mov rdi, %d", node->lhs->ty->ptr_to->size);
      printfln("  idiv rdi");
      break;
    case ND_MUL:
      printfln("  imul rax, rdi");
      break;
    case ND_DIV:
      printfln("  cqo");
      printfln("  idiv rdi");
      break;
    case ND_MOD:
      printfln("  cqo");
      printfln("  idiv rdi");
      printfln("  mov rax, rdx");
      break;
    case ND_LT:
      printfln("  cmp rax, rdi");
      printfln("  setl al");
      printfln("  movzb rax, al");
      break;
    case ND_LTE:
      printfln("  cmp rax, rdi");
      printfln("  setle al");
      printfln("  movzb rax, al");
      break;
    case ND_EQL:
      printfln("  cmp rax, rdi");
      printfln("  sete al");
      printfln("  movzb rax, al");
      break;
    case ND_NOT_EQL:
      printfln("  cmp rax, rdi");
      printfln("  setne al");
      printfln("  movzb rax, al");
      break;
    default:
      error("予期しないNodeです。 kind: %d", node->kind);
  }
  printfln("  push rax");
}

static void codegen_func(Function *func) {
  funcname = func->name;
  if (!func->is_staitc) {
    printfln(".global %s", func->name);
  }
  printfln("%s:", func->name);
  // プロローグ
  // rbp初期化とローカル変数確保
  printfln("  push rbp"); // 前の関数呼び出しでのrbpをスタックに対比
  printfln("  mov rbp, rsp"); // この関数呼び出しでのベースポインタ設定
  // 使用されているローカル変数の数分、領域確保(ここに引数の値を保存する領域も確保される)
  printfln("  sub rsp, %d", func->stack_size);


  // paramsが引数を逆順に保持しているので、ロードするレジスタも逆順にする。そのため一度引数の数を数える
  int param_len = 0;
  for (VarList *v = func->params; v; v = v->next) {
    param_len++;
  }

  // レジスタから引数の情報をスタックに確保
  int i = param_len - 1;
  for (VarList *v = func->params; v; v = v->next) {
    int sz = v->var->type->size;
    if (sz == 1) {
      printfln("  mov [rbp-%d], %s", v->var->offset, ARGUMENT_REGISTERS_SIZE1[i]);
    } else if (sz == 2) {
      printfln("  mov [rbp-%d], %s", v->var->offset, ARGUMENT_REGISTERS_SIZE2[i]);
    } else if (sz == 4) {
      printfln("  mov [rbp-%d], %s", v->var->offset, ARGUMENT_REGISTERS_SIZE4[i]);
    } else if (sz == 8) {
      assert(sz == 8);
      printfln("  mov [rbp-%d], %s", v->var->offset, ARGUMENT_REGISTERS_SIZE8[i]);
    } else {
      assert(false);
    }
    i--;
  }

  // fprintfln(stderr, "func: %s, stack_size: %d", node->name, node->stack_size);
  // 先頭の文からコード生成
  for (Node *n = func->body; n; n = n->next) {
    gen(n);
  }

  // エピローグ
  // rbpの復元と戻り値設定
  // 最後の演算結果が、rax(forの最後でpopしてるやつ)にロードされてるのでそれをmainの戻り値として返す
  printfln(".L.return.%s:", func->name);
  printfln("  mov rsp, rbp");
  printfln("  pop rbp");
  printfln("  ret");
}

static void codegen_data(Program *pgm) {
  printfln(".data");
  for (VarList *v = pgm->global_var; v; v = v->next) {
    Var *var = v->var;

    printfln("%s:", var->name);
    if (var->contents) {
      printf("  .byte");
      for (int i = 0; i < var->content_length; i++) {
        printf(" %d", var->contents[i]);
        if (i + 1 < var->content_length) {
          printf(",");
        }
      }
      printfln("");
    } else {
      printfln("  .zero %d", var->type->size);
    }
  }
}

static void codegen_text(Program *pgm) {
  printfln(".text");
  for (Function *f = pgm->functions; f; f = f->next) {
    codegen_func(f);
  }
}

void codegen(Program *pgm) {
  printfln(".intel_syntax noprefix");

  codegen_data(pgm);
  codegen_text(pgm);
}
