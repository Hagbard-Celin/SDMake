
/*
 *  LISTS.H
 */

struct Node *__asm GetHead(register __a0 struct List *);
struct Node *__asm GetTail(register __a0 struct List *);
struct Node *__asm GetSucc(register __a0 struct Node *);
struct Node *__asm GetPred(register __a0 struct Node *);

