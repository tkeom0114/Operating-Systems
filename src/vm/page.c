#include "vm/page.h"
#include <hash.h>
#include "threads/malloc.h"



static unsigned page_hash_func (const struct hash_elem *e, void *aux)
{
    struct page *p = hash_entry (e,struct page,page_elem);
    return (unsigned long long)(p->virtual_address)>>12;
}

static bool page_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
    return page_hash_func (a,aux) < page_hash_func (b,aux);
}

void page_table_init (struct hash *supp_page_table)
{
    hash_init (supp_page_table,page_hash_func,page_less_func,0);
}

bool insert_page (struct hash *supp_page_table, struct page *p)
{
    struct hash_elem *e = hash_insert (supp_page_table,&p->page_elem);
    return (e == NULL);
}

bool delete_page (struct hash *supp_page_table, struct page *p)
{
    struct hash_elem *e = hash_delete (supp_page_table,&p->page_elem);
    return (e != NULL);
}

struct page *find_page (struct hash *supp_page_table, void *virtual_address)
{
    struct page *p;
    p = malloc (sizeof(struct page));
    p->virtual_address = virtual_address;
    struct hash_elem *e =  hash_find (supp_page_table,&p->page_elem);
    free(p);
    if (e == NULL)
        return NULL;
    return hash_entry (e,struct page,page_elem);
}

void page_destroy_func (struct hash_elem*e, void *aux)
{
    struct page *p = hash_entry (e,struct page,page_elem);
    if (p->physical_address!=NULL)
        palloc_free_page (p->physical_address);
    free (hash_entry (e,struct page,page_elem));
}

void destroy_page_table (struct hash *supp_page_table)
{
    hash_destroy (supp_page_table,page_destroy_func);
}