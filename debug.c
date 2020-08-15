#include "ynicc.h"

char *node_kind_to_s(Node *nd) {
  switch (nd->kind) {
    case  ND_DUMMY:
      return "ND_DUMMY";
    case  ND_ADD:
      return  "ND_ADD";
    case  ND_PTR_ADD:
      return  "ND_PTR_ADD";
    case  ND_SUB:
      return  "ND_SUB";
    case  ND_PTR_SUB:
      return  "ND_PTR_SUB";
    case  ND_PTR_DIFF:
      return  "ND_PTR_DIFF";
    case  ND_MUL:
      return  "ND_MUL";
    case  ND_DIV:
      return  "ND_DIV";
    case  ND_LT:
      return  "ND_LT";
    case  ND_LTE:
      return  "ND_LTE";
    case  ND_EQL:
      return  "ND_EQL";
    case  ND_NOT_EQL:
      return  "ND_NOT_EQ";
    case  ND_ASSIGN:
      return  "ND_ASSIGN";
    case  ND_VAR:
      return  "ND_VAR";
    case  ND_RETURN:
      return  "ND_RETURN";
    case  ND_WHILE:
      return  "ND_WHILE";
    case  ND_BLOCK:
      return  "ND_BLOCK";
    case  ND_IF:
      return  "ND_IF";
    case  ND_FOR:
      return  "ND_FOR";
    case  ND_CALL:
      return  "ND_CALL";
    case  ND_ADDR:
      return  "ND_ADDR";
    case  ND_DEREF:
      return  "ND_DEREF";
    case  ND_VAR_DECL:
      return  "ND_VAR_DECL";
    case  ND_EXPR_STMT:
      return  "ND_EXPR_STMT";
    case  ND_MOD:
      return  "ND_MOD";
    case  ND_NUM:
      return "ND_NUM";
    case  ND_MEMBER:
      return "ND_MEMBER";
    case  ND_CAST:
      return "ND_CAS";
    case  ND_COMMA:
      return "ND_COMMA";
    case  ND_POST_INC:
      return "ND_POST_INC";
    case  ND_POST_DEC:
      return "ND_POST_DEC";
    case  ND_PRE_INC:
      return "ND_PRE_INC";
    case  ND_PRE_DEC:
      return "ND_PRE_DEC";
    case  ND_NOT:
      return "ND_NOT";
    case  ND_BIT_NOT:
      return "ND_BIT_NOT";
    case  ND_BIT_AND:
      return "ND_BIT_AND";
    case  ND_BIT_OR:
      return "ND_BIT_OR";
    case  ND_BIT_XOR:
      return "ND_BIT_XOR";
    case  ND_NULL:
      return "ND_NULL";
  };
}

int type_info_helper(char *buf, int offset, Type *type) {
  int n = 0;

  #pragma clang diagnostic ignored "-Wswitch"
  switch (type->kind) {
    case TY_VOID:
      n += sprintf(buf + offset, "void");
      break;
    case TY_INT:
      n += sprintf(buf + offset, "int");
      break;
    case TY_CHAR:
      n += sprintf(buf + offset, "char");
      break;
    case TY_SHORT:
      n += sprintf(buf + offset, "short");
      break;
    case TY_LONG:
      n += sprintf(buf + offset, "long");
      break;
    case TY_PTR:
      n += sprintf(buf + offset, "*");
      n += type_info_helper(buf, offset + n, type->ptr_to);
      break;
    case TY_ARRAY:
      // int x[2][3]と宣言された配列の場合
      //
      // TY_ARRAY
      // array_size: 2
      // ptr_to ---------> TY_ARRAY
      //                   array_size: 3
      //                   ptr_to   --------> TY_INT
      //
      // と、「「intのサイズ3の配列」のサイズ2の配列」という構造になるので、
      n += sprintf(buf + offset, "[%ld]", type->array_size);
      n += type_info_helper(buf, offset + n, type->ptr_to);
      break;
    case TY_STRUCT:
      n += sprintf(buf + offset, "struct (size %d) {", type->size);
      for (Member *m = type->members; m; m = m->next) {
        n += type_info_helper(buf, offset + n, m->ty);
        n += sprintf(buf + offset + n, " %s (offset %d)", m->name, m->offset);
        if (m->next) {
          n += sprintf(buf + offset + n, " ");
        }
      }
      n += sprintf(buf + offset + n, "}");
      break;
  }
  return n;
}

char *type_info(Type *type) {
  char buf[10000];
  int n = type_info_helper(buf, 0, type);
  return my_strndup(buf, n);
}

char *node_ast(Node *node) {
  char buf[10000];
  int n = 0;

  if (!node) {
    return NULL;
  }

  switch (node->kind) {
      case ND_DUMMY:
        break;
      case ND_ADD:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(+ %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_PTR_ADD:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(ptr+ %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_SUB:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(- %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_PTR_SUB:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(ptr- %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_PTR_DIFF:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(ptr-diff %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_MUL:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(* %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_DIV:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(/ %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_BIT_XOR:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(^ %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_BIT_OR:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(| %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_BIT_AND:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(& %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_LT:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(< %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_LTE:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(<= %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_EQL:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(== %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_NOT_EQL:
        {
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(!= %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_ASSIGN:
        {
          assert(node->lhs);
          assert(node->rhs);
          char *l = node_ast(node->lhs);
          char *r = node_ast(node->rhs);
          n = sprintf(buf, "(let %s %s)", l, r);
          char *ret = my_strndup(buf, n);
          free(l);
          free(r);
          return ret;
        }
      case ND_VAR:
        n = sprintf(buf, "(%cvar %s)", node->var->is_local ? 'l' : 'g', node->var->name);
        return my_strndup(buf, n);
      case ND_RETURN:
        {
          char *l = node_ast(node->lhs);
          n = sprintf(buf, "(return %s)", l);
          free(l);
          return my_strndup(buf, n);
        }
      case ND_WHILE:
        {
          char *cond = node_ast(node->cond);
          char *body = node_ast(node->body);
          n = sprintf(buf, "(while (cond %s) (body %s))", cond, body);
          char *ret = my_strndup(buf, n);
          free(cond);
          free(body);
          return ret;
        }
      case ND_BLOCK:
        {
          int i = 0;
          char *tmp_bufs[1000];
          n += sprintf(buf, "(block ");
          for (Node *nd = node->body; nd; nd = nd->next) {
            tmp_bufs[i] = node_ast(nd);
            n += sprintf(buf + n, "%s", tmp_bufs[i]);
            if (nd->next) {
              n += sprintf(buf + n, " ");
            }
            i++;
          }
          for (int j = 0; j < i; j++) {
            free(tmp_bufs[j]);
          }
          n += sprintf(buf + n, ")");
          return my_strndup(buf, n);
        }
      case ND_IF:
        {
          char *cond = node_ast(node->cond);
          char *then = node_ast(node->then);
          char *els = node_ast(node->els);
          n = sprintf(buf, "(if (cond %s) (then %s) (else %s))", node_ast(node->cond), node_ast(node->then), node_ast(node->els));
          char *ret = my_strndup(buf, n);
          free(cond);
          free(then);
          free(els);
          return ret;
        }
      case ND_FOR:
        {
          char *init = node_ast(node->init);
          char *cond = node_ast(node->cond);
          char *inc = node_ast(node->inc);
          char *body = node_ast(node->body);
          n = sprintf(buf, "(for (init %s) (cond %s) (inc %s) (body %s))", node_ast(node->init), node_ast(node->cond), node_ast(node->inc), node_ast(node->body));
          char *ret = my_strndup(buf, n);
          free(init);
          free(cond);
          free(inc);
          free(body);
          return ret;
        }
      case ND_CALL:
        {
          char *tmp_bufs[1000];
          n = sprintf(buf, "(call (name %s)", node->funcname);
          int i = 0;
          for (Node *p = node->arg; p; p = p->next) {
            char *tmp = tmp_bufs[i] = node_ast(p);
            n += sprintf(buf + n, " %s", tmp);
            i++;
          }
          n += sprintf(buf + n, ")");
          char *ret = my_strndup(buf, n);

          for (int j = 0; j < i; j++) {
            free(tmp_bufs[j]);
          }
          return ret;
        }
      case ND_ADDR:
        {
          char *l = node_ast(node->lhs);
          n = sprintf(buf, "(addr %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_DEREF:
        {
          char *l = node_ast(node->lhs);
          n = sprintf(buf, "(deref %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_VAR_DECL:
        {
          char *ti = type_info(node->var->type);
          n += sprintf(buf, "(decl (%s %s)", ti, node->var->name);
          if (node->initializer) {
            n += sprintf(buf + n, " (init %s)", node_ast(node->initializer));
          }
          n += sprintf(buf + n, ")");
          char *ret = my_strndup(buf, n);
          free(ti);
          return ret;
        }
      case ND_EXPR_STMT:
        {
          char *l = node_ast(node->lhs);
          n += sprintf(buf, "(expr-stmt %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_NUM:
        n += sprintf(buf, "(num %ld)", node->val);
        return my_strndup(buf, n);
      case ND_MEMBER:
        {
          char *l = node_ast(node->lhs);
          n += sprintf(buf, "(member %s %s)", l, node->member->name);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_NOT:
        {
          char *l = node_ast(node->lhs);
          n += sprintf(buf, "(! %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_BIT_NOT:
        {
          char *l = node_ast(node->lhs);
          n += sprintf(buf, "(~ %s)", l);
          char *ret = my_strndup(buf, n);
          free(l);
          return ret;
        }
      case ND_NULL:
        return "(ND_NULL)";
  }
  return NULL;
}

char *node_ast_helper(Node *body) {
  char *tmp_bufs[1000];// 文の数
  char buf[10000];// ast dumpの文字列バッファ

  int n = 0;
  int i = 0;

  n += sprintf(buf + n, "\n");
  for (Node *nd = body; nd; nd = nd->next) {
    char *tmp = tmp_bufs[i] = node_ast(nd);
    n += sprintf(buf + n, "##   %s\n", tmp);
  }
  char *ret = my_strndup(buf, n);
  for (int j = 0; j < i; j++) {
    free(tmp_bufs[j]);
  }
  return ret;
}

char *function_body_ast(Function *f) {
  int n = 0;
  char buf[10000];

  char *ti = type_info(f->return_type);
  n = sprintf(buf + n, "(def (%s) %s (", ti, f->name);
  free(ti);

  int i = 0;
  VarList* reverse_params = NULL;
  // f->paramsは最後の引数からのリンクリストになってるのでダンプ用には逆順にする。
  for (VarList *cur = f->params; cur; cur = cur->next) {
    VarList *vl = calloc(1, sizeof(VarList));
    vl->next = reverse_params;
    vl->var = cur->var;
    reverse_params = vl;
  }

  // 逆順にしたリストからastダンプ
  for (VarList *p = reverse_params; p; p = p->next) {
    char *ti = type_info(p->var->type);
    n += sprintf(buf + n, "%s %s", ti, p->var->name);
    free(ti);
    if (p->next) {
      n += sprintf(buf + n, " ");
    }
  }
  // 逆順のリストはもういらないので捨てる。(そこに紐づくLVar は別途freeされるので無視する)
  VarList *rp = reverse_params;
  while (rp) {
    VarList *tmp = rp->next;
    free(rp);
    rp = tmp;
  }

  char *body_ast = node_ast_helper(f->body);
  n += sprintf(buf + n, ") ( %s", body_ast);
  n += sprintf(buf + n, "## ))");
  free(body_ast);

  return my_strndup(buf, n);
}

void dump_function(Function *f) {
  char *ast = function_body_ast(f);
  printf("## %s\n", ast);
  free(ast);
}

void dump_globals(VarList *vars) {
  VarList *v = vars;
  while (v) {
    Node *dummy = calloc(1, sizeof(Node));
    dummy->kind = ND_VAR_DECL; // ローカル変数の宣言のダンプを使い回す
    dummy->var = v->var;
    printf("## %s\n", node_ast(dummy));
    v = v->next;
  }
}

void dump_tokens(Token *t) {
  while (t) {
    dump_token(t);
    t = t->next;
  }
}

