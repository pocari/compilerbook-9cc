#include "ynicc.h"

Type *int_type = &(Type){ TY_INT };

Type *new_type(TypeKind kind, Type *ptr_to) {
  Type *t = calloc(1, sizeof(Type));
  t->kind = kind;
  t->ptr_to = ptr_to;

  return t;
}

Type *pointer_to(Type *ptr_to) {
  return new_type(TY_PTR, ptr_to);
}

bool is_integer(Type *t) {
  return t->kind == TY_INT;
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
    case ND_LVAR:
    case ND_CALL:
    case ND_NUM:
      node->ty = int_type;
      return;
    case ND_PTR_ADD:
    case ND_PTR_SUB:
    case ND_ASSIGN:
      node->ty = node->lhs->ty;
      return;;
    case ND_ADDR:
      node->ty = pointer_to(node->lhs->ty);
      return;
    case ND_DEREF:
      if (node->lhs->ty->kind == TY_PTR) {
      node->ty = node->lhs->ty->ptr_to;
      } else {
      node->ty = int_type;
      }
      return;
  }

}
