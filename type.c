#include "ynicc.h"

Type *int_type = &(Type){ TY_INT, 8, 8};
Type *char_type = &(Type){ TY_CHAR, 1, 1};

Type *new_type(TypeKind kind, Type *ptr_to, int size, int align) {
  Type *t = calloc(1, sizeof(Type));
  t->kind = kind;
  t->size = size;
  t->align = align;
  t->ptr_to = ptr_to;

  return t;
}

Type *pointer_to(Type *ptr_to) {
  return new_type(TY_PTR, ptr_to, 8, 8);
}

Type *array_of(Type *ptr_to, int array_size) {
  Type *ty = new_type(TY_ARRAY, ptr_to, ptr_to->size * array_size, ptr_to->align);
  ty->array_size = array_size;
  return ty;
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
    case ND_CALL:
    case ND_NUM:
      node->ty = int_type;
      return;
    case ND_PTR_ADD:
    case ND_PTR_SUB:
    case ND_ASSIGN:
      node->ty = node->lhs->ty;
      return;
    case ND_VAR:
      node->ty = node->var->type;
      return;
    case ND_MEMBER:
      node->ty = node->member->ty;
      return;
    case ND_ADDR:
      if (node->lhs->ty->kind == TY_ARRAY) {
        node->ty = pointer_to(node->lhs->ty->ptr_to);
      } else {
        node->ty = pointer_to(node->lhs->ty);
      }
      return;
    case ND_DEREF:
      if (is_pointer(node->lhs->ty)) {
        node->ty = node->lhs->ty->ptr_to;
      } else {
        node->ty = int_type;
      }
      // chibicc だと、こうなっていたが、yniccでやるとエラーになってしまった・・・。
      // if (!node->lhs->ty->ptr_to) {
      //   error("invalid pointer dereferrence");
      // }
      // node->ty = node->lhs->ty->ptr_to;
      return;
  }

}
