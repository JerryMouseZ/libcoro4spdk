#ifndef RCU_LIST_H
#define RCU_LIST_H
struct ListNode {
  ListNode* next;
  ListNode* prev;
  void* data;
};

static inline void list_add(struct ListNode* newp, struct ListNode* head) {
  head->next->prev = newp;
  newp->next = head->next;
  newp->prev = head;
  head->next = newp;
}

static inline void list_add_tail(struct ListNode* newp, struct ListNode* head) {
  head->prev->next = newp;
  newp->next = head;
  newp->prev = head->prev;
  head->prev = newp;
}

/* Remove element from list. */
static inline void __list_del(struct ListNode* prev, struct ListNode* next) {
  next->prev = prev;
  prev->next = next;
}

/* Remove element from list. */
static inline void cds_list_del(struct ListNode* elem) {
  __list_del(elem->prev, elem->next);
}

/* Remove element from list, initializing the element's list pointers. */
static inline void cds_list_del_init(struct ListNode* elem) {
  cds_list_del(elem);
  CDS_INIT_LIST_HEAD(elem);
}

/* Delete from list, add to another list as head. */
static inline void cds_list_move(struct ListNode* elem, struct ListNode* head) {
  __list_del(elem->prev, elem->next);
  list_add(elem, head);
}

/* Replace an old entry. */
static inline void cds_list_replace(struct ListNode* old,
                                    struct ListNode* _new) {
  _new->next = old->next;
  _new->prev = old->prev;
  _new->prev->next = _new;
  _new->next->prev = _new;
}

/* Join two lists. */
static inline void cds_list_splice(struct ListNode* add,
                                   struct ListNode* head) {
  /* Do nothing if the list which gets added is empty. */
  if (add != add->next) {
    add->next->prev = head;
    add->prev->next = head->next;
    head->next->prev = add->prev;
    head->next = add->next;
  }
}

/* Get typed element from list at a given position. */
#define cds_list_entry(ptr, type, member) caa_container_of(ptr, type, member)

/* Get first entry from a list. */
#define cds_list_first_entry(ptr, type, member) \
  cds_list_entry((ptr)->next, type, member)

/* Iterate forward over the elements of the list. */
#define cds_list_for_each(pos, head) \
  for (pos = (head)->next; (pos) != (head); pos = (pos)->next)

/*
 * Iterate forward over the elements list. The list elements can be
 * removed from the list while doing this.
 */
#define cds_list_for_each_safe(pos, p, head)                 \
  for (pos = (head)->next, p = (pos)->next; (pos) != (head); \
       pos = (p), p = (pos)->next)

/* Iterate backward over the elements of the list. */
#define cds_list_for_each_prev(pos, head) \
  for (pos = (head)->prev; (pos) != (head); pos = (pos)->prev)

/*
 * Iterate backwards over the elements list. The list elements can be
 * removed from the list while doing this.
 */
#define cds_list_for_each_prev_safe(pos, p, head)            \
  for (pos = (head)->prev, p = (pos)->prev; (pos) != (head); \
       pos = (p), p = (pos)->prev)

#define cds_list_for_each_entry(pos, head, member)                     \
  for (pos = cds_list_entry((head)->next, __typeof__(*(pos)), member); \
       &(pos)->member != (head);                                       \
       pos = cds_list_entry((pos)->member.next, __typeof__(*(pos)), member))

#define cds_list_for_each_entry_reverse(pos, head, member)             \
  for (pos = cds_list_entry((head)->prev, __typeof__(*(pos)), member); \
       &(pos)->member != (head);                                       \
       pos = cds_list_entry((pos)->member.prev, __typeof__(*(pos)), member))

#define cds_list_for_each_entry_safe(pos, p, head, member)                \
  for (pos = cds_list_entry((head)->next, __typeof__(*(pos)), member),    \
      p = cds_list_entry((pos)->member.next, __typeof__(*(pos)), member); \
       &(pos)->member != (head); pos = (p),                               \
      p = cds_list_entry((pos)->member.next, __typeof__(*(pos)), member))

/*
 * Same as cds_list_for_each_entry_safe, but starts from "pos" which should
 * point to an entry within the list.
 */
#define cds_list_for_each_entry_safe_from(pos, p, head, member)            \
  for (p = cds_list_entry((pos)->member.next, __typeof__(*(pos)), member); \
       &(pos)->member != (head); pos = (p),                                \
      p = cds_list_entry((pos)->member.next, __typeof__(*(pos)), member))

static inline int cds_list_empty(struct ListNode* head) {
  return head == head->next;
}

static inline void cds_list_replace_init(struct ListNode* old,
                                         struct ListNode* _new) {
  struct ListNode* head = old->next;

  cds_list_del(old);
  cds_list_add_tail(_new, head);
  CDS_INIT_LIST_HEAD(old);
}

#endif
