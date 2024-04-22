struct cds_list_head {
  struct cds_list_head *next, *prev;
};

/* Define a variable with the head and tail of the list. */
#define CDS_LIST_HEAD(name) struct cds_list_head name = {&(name), &(name)}

/* Initialize a new list head. */
#define CDS_INIT_LIST_HEAD(ptr) (ptr)->next = (ptr)->prev = (ptr)

#define CDS_LIST_HEAD_INIT(name) \
  { .next = &(name), .prev = &(name) }
