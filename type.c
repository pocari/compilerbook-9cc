#include "ynicc.h"

Type *void_type = &(Type){ TY_VOID, 1, 1};
Type *bool_type = &(Type){ TY_BOOL, 1, 1};
Type *int_type = &(Type){ TY_INT, 4, 4};
Type *char_type = &(Type){ TY_CHAR, 1, 1};
Type *long_type = &(Type){ TY_LONG, 8, 8};
Type *short_type = &(Type){ TY_SHORT, 2, 2};

static Type *new_type(TypeKind kind, int size, int align) {
  Type *t = calloc(1, sizeof(Type));
  t->kind = kind;
  t->size = size;
  t->align = align;

  return t;
}

Type *pointer_to(Type *ptr_to) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->ptr_to = ptr_to;
  return ty;
}

Type *array_of(Type *ptr_to, int array_size) {
  Type *ty = new_type(TY_ARRAY, ptr_to->size * array_size, ptr_to->align);
  ty->ptr_to = ptr_to;
  ty->array_size = array_size;
  return ty;
}

Type *func_type(Type *return_type) {
  Type *ty = new_type(TY_FUNC, 1, 1);
  ty->return_ty = return_type;
  return ty;
}

Type *enum_type(void) {
  return new_type(TY_ENUM, 4, 4);
}

Type *struct_type(void) {
  // 一旦サイズゼロ、アラインメント1バイトで作成
  // 実際には構造体のメンバーのパース時に、メンバー中で一番大きいアラインメントで上書きされる。
  Type *type = new_type(TY_STRUCT, 0, 1);
  type->is_incomplete = true;
  return type;
}


bool is_pointer(Type *t) {
  return t->kind == TY_PTR || t->kind == TY_ARRAY;
}

bool is_integer(Type *t) {
  return !is_pointer(t);
}

int node_type_size(Node *node) {
  return node->ty->size;
}

void add_type(Node *node) {
  if (!node || node->ty)
    return;

  assert(node);
  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);
  add_type(node->init);
  add_type(node->inc);

  for (Node *n = node->body; n; n = n->next) {
    add_type(n);
  }
  for (Node *n = node->arg; n; n = n->next) {
    add_type(n);
  }
  #pragma clang diagnostic ignored "-Wswitch"
  switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_PTR_DIFF:
    case ND_MUL:
    case ND_DIV:
    case ND_EQL:
    case ND_NOT_EQL:
    case ND_LT:
    case ND_LTE:
    case ND_BIT_AND:
    case ND_BIT_OR:
    case ND_BIT_XOR:
    case ND_A_LSHIFT:
    case ND_A_RSHIFT:
    case ND_NUM:
      node->ty = long_type;
      return;
    case ND_PTR_ADD:
    case ND_PTR_SUB:
    case ND_ASSIGN:
    case ND_POST_INC:
    case ND_POST_DEC:
    case ND_PRE_INC:
    case ND_PRE_DEC:
      node->ty = node->lhs->ty;
      return;
    case ND_VAR:
      node->ty = node->var->type;
      return;
    case ND_MEMBER:
      node->ty = node->member->ty;
      return;
    case ND_TERNARY:
      node->ty = node->then->ty;
      return;
    case ND_ADDR:
      if (node->lhs->ty->kind == TY_ARRAY) {
        node->ty = pointer_to(node->lhs->ty->ptr_to);
      } else {
        node->ty = pointer_to(node->lhs->ty);
      }
      return;
    case ND_DEREF:
      {
        if (!node->lhs->ty->ptr_to) {
          error_at(node->tok->str, "invalid pointer dereferrence");
        }
        Type *ty = node->lhs->ty->ptr_to;
        if (ty->kind == TY_VOID) {
          error_at(node->tok->str, "void *ポインタをデリファレンスできません");
        }
        if (ty->kind == TY_STRUCT && ty->is_incomplete) {
          error_at(node->tok->str, "不完全な構造体のデリファレンスはできません");
        }
        node->ty = ty;
      }
      return;
  }

}
