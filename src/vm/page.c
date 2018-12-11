#include "vm/page.h"
#include <hash.h>
#include <list.h>
#include <bitmap.h>
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/exception.h"
#include "userprog/syscall.h"
#include "devices/block.h"
#include <stdio.h>



static unsigned page_hash_func (const struct hash_elem *e, void *aux)
{
    struct page *p = hash_entry (e,struct page,page_elem);
    return (unsigned long long)(p->virtual_address) >> PGBITS;
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
    if(p->physical_address != NULL)
    {
        list_remove (&p->frame_elem);
    }
    free(p);
    return (e != NULL);
}

struct page *find_page (struct hash *supp_page_table, void *virtual_address)
{
    /*printf("[ page.c / find_page ] :: uva to find: %d\n",(int)virtual_address);*/
    struct page *p;
    p = malloc (sizeof(struct page));
    p->virtual_address = virtual_address;
    struct hash_elem *e =  hash_find (supp_page_table,&p->page_elem);
    free(p);
    if (e == NULL)
        return NULL;
    /*printf("[ page.c / find_page ] :: found uva : %d\n",(int)virtual_address);*/
    return hash_entry (e,struct page,page_elem);
}

void page_destroy_func (struct hash_elem *e, void *aux)
{
    struct page *p = hash_entry (e,struct page,page_elem);
    if(p->physical_address != NULL)
    {
        list_remove (&p->frame_elem);
    }
    free(p);
}
//pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/vm/mmap-overlap -a mmap-overlap -p tests/vm/zeros -a zeros --swap-size=4 -- -q  -f run mmap-overlap

void destroy_page_table (struct hash *supp_page_table)
{
    hash_destroy (supp_page_table,page_destroy_func);
}

//grow stack
struct page* grow_stack (void *ptr, void *esp)
{
    if (ptr < esp-32 || ptr >= PHYS_BASE
      || ptr < PHYS_BASE-0x00400000)
        return NULL;
    uint8_t *kpage = palloc_get_page (PAL_USER | PAL_ZERO);
    void *vpage;
    if (kpage == NULL)
    {
        kpage = evict_page (PAL_USER | PAL_ZERO);
        if(kpage == NULL)
        {
          //printf ("Failed!\n");//debugging
          return NULL;
        }
    }
        
    vpage = (void*)(((int)ptr>>PGBITS)<<PGBITS);
    /*printf("{vm/page.c/grow_stack] :: vpage val : %d",(int)vpage);*/
    if (!install_page (vpage, kpage, true))
    {
        palloc_free_page (kpage);
        return NULL;
    }
    struct page *p = malloc (sizeof (struct page));
    if (p == NULL)
        return NULL;
    p->type = SWAP_PAGE;
    p->file = NULL;
    p->virtual_address = vpage;
    p->physical_address = kpage;
    p->writable = true;
    p->offset = 0;
    p->read_bytes = 0;
    p->zero_bytes = PGSIZE;
    p->swap_slot = -1;
    if(!insert_page (&thread_current ()->supp_page_table,p))
    {
        free(p);
        palloc_free_page (kpage);
        return NULL;
    }
    list_push_back (&frame_list,&p->frame_elem);
    return p;
}

bool add_mmap_to_page_table(struct file *file, int32_t offset, uint8_t *upage,
        uint32_t read_bytes, uint32_t zero_bytes){
    /*printf("[ page.c / add_mmap_to_page_table ] :: read_bytes : %d\n",read_bytes);*/
    struct page *p = malloc(sizeof(struct page));
    if(!p){
        return false;
    }
    p->file = file;
    p->offset = offset;
    p->virtual_address = upage;
    p->physical_address = NULL;
    p->writable = true;
    p->read_bytes = read_bytes;
    p->zero_bytes = zero_bytes;
    p->type = MMAP_PAGE;
    if(!process_add_mmap(p)){
        /*printf("[ page.c / add_mmap_to_page_table ] :: fail\n");*/
        free(p);
        return false;
    }
    if (!insert_page (&thread_current()->supp_page_table, p)){
        return false;
    }
    //printf("[ page.c / add_mmap_to_page_table ] :: success\n");
    return true;
}

//pintos -v -k -T 600 --qemu  --filesys-size=2 -p tests/vm/page-merge-par -a page-merge-par -p tests/vm/child-sort -a child-sort --swap-size=4 -- -q  -f run page-merge-par

//pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/vm/page-merge-mm -a page-merge-mm -p tests/vm/child-qsort-mm -a child-qsort-mm --swap-size=4 -- -q  -f run page-merge-mm

 uint8_t *evict_page (uint8_t flag)
 {
    
     //printf ("Failed1!\n");//debugging
    if(list_empty (&frame_list))
    {
        return NULL;
    }    
    //lock_acquire (&evict_lock);  
    struct list_elem *clock=list_begin(&frame_list);
    while(true)
    {       
        if(clock==list_end(&frame_list))
        {
            clock = list_begin (&frame_list);
        }
        //printf ("Failed2!\n");//debugging
        struct page *p = list_entry (clock,struct page,frame_elem);
        if (pagedir_is_accessed (thread_current()->pagedir,p->virtual_address))
        {
            //printf ("Failed3!\n");//debugging
            pagedir_set_accessed (thread_current()->pagedir,p->virtual_address,false);    
        } 
        else
        {
            if(pagedir_is_dirty (thread_current()->pagedir,p->virtual_address) || p->type==SWAP_PAGE)
            {
                //printf ("Failed4!\n");//debugging
                if (p->type==MMAP_PAGE)
                {
                    lock_acquire (&file_lock);
                    file_write_at (p->file,p->physical_address,p->read_bytes,p->offset);
                    lock_release (&file_lock);
                }
                else
                {
                    p->type=SWAP_PAGE;
                    size_t slot = bitmap_scan_and_flip (swap_table,0,1,false);
                    for(size_t i=0;i<8;i++)
                        block_write (swap_block,8*slot+i,p->physical_address+i*BLOCK_SECTOR_SIZE);
                    p->swap_slot=slot;
                    //printf ("Failed5!\n");//debugging
                }     
            }
            list_remove (&p->frame_elem);
            pagedir_clear_page (thread_current ()->pagedir,p->virtual_address);
            palloc_free_page (p->physical_address);         
            p->physical_address=NULL;
            //printf ("Failed6!\n");//debugging
            break;
            //lock_acquire (&evict_lock);
        }
        clock = list_next (clock);
    }
    
    return palloc_get_page (flag);

 }

bool get_frame(void *fault_addr,void *esp,bool write)
{
    
    struct page *p = find_page (&thread_current ()->supp_page_table,fault_addr);
    bool success = false;
    //stack growth(added 11/28 18:10)
    if (p == NULL )
    {
      if (grow_stack (fault_addr,esp))
      {
          return true;
      }  
      else
      {
        return false;
      }
        
    }
    if (!p->writable && write)
        return false;
    if (p->physical_address!=NULL)
        return true;
        
    //load execute file
    if(p->type == EXE_PAGE)
    {
        //printf ("Failed!\n");//debugging
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if(kpage == NULL)
      {
        kpage = evict_page (PAL_USER);
        if(kpage == NULL)
        {
          return false;
        }
      }
    //pintos -v -k -T 300 --qemu  --filesys-size=2 -p tests/vm/page-linear -a page-linear --swap-size=4 -- -q  -f run page-linear
      success = install_page (p->virtual_address,kpage,p->writable);
      if (!success)
      {
        palloc_free_page (kpage);
        return false;
      }     
      p->physical_address = kpage;
      lock_acquire (&file_lock);
      size_t result = file_read_at (p->file,kpage,p->read_bytes,p->offset);
      lock_release (&file_lock);
      if (result != p->read_bytes)
      {
        return false;
      }
        
      memset (kpage + p->read_bytes, 0, p->zero_bytes);
      list_push_back (&frame_list,&p->frame_elem);
    }
    else if(p->type == MMAP_PAGE){
      uint8_t *kpage = palloc_get_page(PAL_USER);
      if(kpage == NULL)
      {
        //printf ("Failed!\n");//debugging
        kpage = evict_page (PAL_USER);
        if(kpage == NULL)
        {
          //printf ("Failed!\n");//debugging
          return false;
        }
      }
       /*printf("[ exception.c / page_fault ] :: p->read_bytes = %d\n",p->read_bytes);*/
       success = install_page (p->virtual_address, kpage, p->writable);
       /*printf("[ exception.c / page_fault ] :: p->read_bytes = %d\n",p->read_bytes);*/
       if(!success){
           palloc_free_page(kpage);
           return false;
       }       
       p->physical_address = kpage;
       /*printf("[ exception.c / page_fault ] :: p->read_bytes = %d\n",p->read_bytes);*/
       /*printf("[ exception.c / page_fault ] :: p->offset     = %d\n",p->offset);*/
       lock_acquire (&file_lock);
        size_t result = file_read_at(p->file,kpage,p->read_bytes,p->offset);
        lock_release (&file_lock);
        if (result != p->read_bytes)
        {
         return false;
        }
           
       memset(kpage + p->read_bytes, 0, p->zero_bytes);
       list_push_back (&frame_list,&p->frame_elem);
    }
    else if(SWAP_PAGE)
    {
      uint8_t *kpage = palloc_get_page (PAL_USER | PAL_ZERO);
      if(kpage == NULL)
      {
        //printf ("Failed!\n");//debugging
        kpage = evict_page (PAL_USER | PAL_ZERO);
        if(kpage == NULL)
        {
          //printf ("Failed!\n");//debugging
          return false;
        }
          
      }
      success = install_page (p->virtual_address,kpage,p->writable);
      if (!success)
      {
        palloc_free_page (kpage);
        return false;
      }
      bitmap_flip (swap_table,p->swap_slot);
      for(int i=0;i<8;i++)
        block_read (swap_block,8*p->swap_slot+i,kpage+i*BLOCK_SECTOR_SIZE);
      p->swap_slot = -1;
      p->physical_address = kpage;
      list_push_back (&frame_list,&p->frame_elem);
    }
    return success;
}