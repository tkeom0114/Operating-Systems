#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"


#ifdef VM
  #include "vm/page.h"
#endif
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
struct thread *get_child_thread (tid_t child_tid);
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created.
   Fixed by Taekang Eom
   Time: 10/07  11:32*/
tid_t
process_execute (const char *file_name)
{
    /*printf("[ process.c / process_execute ] :: start\n");*/
  char *fn_copy,*save_ptr,*real_file_name;//fixed at 10/07 11:27
  tid_t tid;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  /*printf("[ process.c / process_execute ] :: before palloc\n");*/
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL){
      /*printf("[ process.c / process_execute ] :: making copy failed\n");*/
    return TID_ERROR;
  }
  strlcpy (fn_copy, file_name, PGSIZE);
  real_file_name = (char *) malloc (strlen (file_name) + 1);
  if (real_file_name == NULL)
  {
      /*printf("[ process.c / process_execute ] :: allocating real_file_name failed\n");*/
    palloc_free_page (fn_copy);
    return TID_ERROR;
  }
  strlcpy (real_file_name, file_name, PGSIZE);
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (strtok_r(real_file_name," ",&save_ptr), PRI_DEFAULT, start_process, fn_copy);//fixed at 10/07 11:32
  free (real_file_name);//added at 11/09 22:47
  if (tid == TID_ERROR){
      /*printf("[ process.c / process_execute ] :: tid==TID_ERROR\n");*/
    palloc_free_page (fn_copy);
  }
  struct thread *t = get_child_thread (tid);
  //waiting child prcess until loaded. added at 10/29 17:08
  sema_down(&t->sema_load);
  //if child process load fail, call wait. added at 11/02 06:55
  if (t->load_success == false)
    return process_wait (tid);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  char *save_ptr;
  char *real_file_name;
  struct intr_frame if_;
  bool success;
  /* Initialize interrupt frame and load executable. */
  #ifdef VM
    page_table_init (&thread_current ()->supp_page_table);//added at 11/27 19:06
    list_init(&thread_current()->mmap_list);
  #endif
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  thread_current ()->load_success = success;//save whether load process is successed. added at 11/12 21:46
  /* If load failed, quit.
  Fixed at 10/30 10:09 */
  if (!success)
  {
    palloc_free_page (file_name);
    sys_exit(-1);
  }
  //deny write execute file. added at 11/02 05:11
  real_file_name = strtok_r(file_name," ",&save_ptr);
  thread_current ()->execute_file = filesys_open(real_file_name);
  file_deny_write (thread_current ()->execute_file);
  palloc_free_page (file_name);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/*Return pointer of thread which tid is child_tid
Author: Taekang Eom
Time: 10/29 19:05*/
struct thread *
get_child_thread (tid_t child_tid)
{
  struct thread *t = thread_current ();
  for (struct list_elem *e = list_begin (&t->child_list); e != list_end (&t->child_list);
       e = list_next (e))
  {
    struct thread *cur = list_entry (e,struct thread,child_elem);
    if(cur->tid == child_tid)
      return cur;
  }
  return NULL;
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing.
   Fixed by Taekang Eom
   Time: 09/27 21:53 */
int
process_wait (tid_t child_tid)
{
  struct thread *t = get_child_thread (child_tid);
  if (t == NULL || t->parent != thread_current() || t->wait_called)
    return -1;
  t->wait_called = true;
  sema_down (&t->sema_wait);
  int status = t->exit_status;
  list_remove (&t->child_elem);
  sema_up (&t->sema_remove);
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
    /*printf("[ userprog / process_exit ] :: start\n");*/
  struct thread *cur = thread_current ();
  uint32_t *pd;
  //erase all childs and wake up parent added at 10/29 22:51
  struct list_elem *e = list_begin (&cur->child_list);
    //fixed at 11/02 15:18
  for (int i = 2;i<cur->next_fd;i++)
  {
    if(cur->file_table[i] != NULL)
    {
      file_close(get_file (i));
    }
  }
  //wait all children to exit. added at 11/11 20:17
  for (struct list_elem *e = list_begin (&cur->child_list);e != list_end (&cur->child_list);e = list_next (e))
  {
    struct thread *t = list_entry (e,struct thread,child_elem);
    process_wait (t->tid);
  }
  free (cur->file_table);
  /*allow write to execute file of this process
  and close this file
  added at 11/02 05:12 */
  if(cur->execute_file != NULL)
  {
    file_allow_write (cur->execute_file);
    file_close (cur->execute_file);
  }
  /*printf("[ process.c / process_exit ] :: destroy_page_table\n");*/
#ifdef VM
   /* struct hash_iterator i;//debugging
    hash_first (&i, &thread_current()->supp_page_table);
      while (hash_next (&i))
        {
          struct page *p = hash_entry (hash_cur (&i), struct page, page_elem);
          printf("virtual address:%lx\n",p->virtual_address);
          printf("physical address:%lx\n",p->physical_address);
        }*/

    process_remove_mmap(-1);
    destroy_page_table(&(cur->supp_page_table));
#endif


  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  sema_up (&cur->sema_wait);
  if(cur->parent != NULL)
    sema_down (&cur->sema_remove);

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp,char *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise.
   Fixed by Taekang Eom
   Time: 10/29 17:24*/
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  //added at 10/07 17:18
  char *real_file_name = malloc(strlen(file_name)+1);
  char *save_ptr;
  strlcpy (real_file_name, file_name, PGSIZE);
  real_file_name = strtok_r(real_file_name," ",&save_ptr);//token file name

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (real_file_name);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", real_file_name);//fixed at 10/07 17:18
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", real_file_name);//fixed at 10/07 17:18
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }

              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */

  if (!setup_stack (esp,file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  //wakeup parent process when child process end loading(whether load is successful or not). added at 10/29 17:18
  sema_up(&thread_current ()->sema_load);
  return success;
}

/* load() helpers. */

bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs.
   Fixed by Taekang Eom
   Time: 11/27 19:32*/
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  off_t load_ofs = ofs;//added at 11/27 19:31
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
    #ifndef VM
      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }
    #else//added at 11/27 19:31
      struct page *p = malloc (sizeof (struct page));
      if (p == NULL)
        return false;
      p->type = EXE_PAGE;
      p->file = file;
      p->virtual_address = upage;
      p->physical_address = NULL;
      p->writable = writable;
      p->offset = load_ofs;
      p->read_bytes = page_read_bytes;
      p->zero_bytes = page_zero_bytes;
      p->swap_slot = -1;
      load_ofs += PGSIZE;
      if(!insert_page (&thread_current ()->supp_page_table,p))
      {
        free(p);
        return false;
      }
    #endif

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory.
   Fixed by Taekang Eom
   Time: 11/27 19:46*/
static bool
setup_stack (void **esp, char *file_name)
{
  uint8_t *kpage;
  bool success = false;
  //added at 10/07 17:32
  int argc = 0;
  int i = 0;
  char **parsed_tokens,**argv;
  char *token;
  char *fn_copy_1;
  char *fn_copy_2;
  char *save_ptr_1;
  char *save_ptr_2;//added
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
#ifdef VM//added at 11/27 19:46
  struct page *p = malloc (sizeof (struct page));
  if (p == NULL)
    return false;
  p->type = SWAP_PAGE;
  p->file = NULL;
  p->virtual_address = ((uint8_t *) PHYS_BASE) - PGSIZE;
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
    return false;
  }
  lock_acquire (&frame_lock);
  list_push_back(&frame_list,&p->frame_elem);
  lock_release (&frame_lock);
  success = true;
#endif
  /*Push arguments to stack
  added at 10/07 17:32*/
  if(success)
  {
    fn_copy_1 = (char*)malloc(strlen(file_name)+1);
    strlcpy (fn_copy_1, file_name, PGSIZE);
    fn_copy_2 = (char*)malloc(strlen(file_name)+1);
    strlcpy (fn_copy_2, file_name, PGSIZE);
    for (token = strtok_r(fn_copy_1," ",&save_ptr_1);token != NULL;token = strtok_r(NULL," ",&save_ptr_1))
      argc++;
    free (fn_copy_1);
    parsed_tokens = calloc (argc,sizeof(char*));
    for (token = strtok_r(fn_copy_2," ",&save_ptr_2);token != NULL;token = strtok_r(NULL," ",&save_ptr_2))
    {
      parsed_tokens[i] = malloc (strlen (token) + 1);
      strlcpy (parsed_tokens[i], token, strlen (token) + 1);
      i++;
    }
    free (fn_copy_2);
    argv = malloc ((argc + 1) * sizeof(char*));
    argv[argc] = NULL;
    for(i = argc - 1;i >= 0; i--)
    {
      int length = strlen(parsed_tokens[i]) + 1;
      *esp -= length;
      memcpy (*esp,parsed_tokens[i],length);
      argv[i] = *esp;
    }
    i = (int32_t)*esp % sizeof(char*);
    if(i)
    {
      *esp -= i;
      memcpy(*esp,&argv[argc],i);
    }
    for(i = argc;i >= 0; i--)
    {
      *esp -= sizeof(char *);
      memcpy(*esp,&argv[i],sizeof(char*));
    }
    token = *esp;
    *esp -= sizeof(char**);
    memcpy(*esp,&token,sizeof(char**));
    *esp -= sizeof(int);
    memcpy(*esp,&argc,sizeof(int));
    *esp -= sizeof(void*);
    memcpy(*esp,&argv[argc],sizeof(void*));
    //free memory of used strings
    for(i=0;i<argc;i++)//added at 11/09 22:50
      free (parsed_tokens[i]);
    free (argv);
    //added
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

bool process_add_mmap(struct page *page){
    /*printf("[ process.c / process_add_mmap ] :: inside add_mmap\n");*/
    struct mmap_file *mm = malloc(sizeof(struct mmap_file));
    if(!mm){
        /*printf("[ userprog / process.c ] :: allocating mmap failed\n");*/
        return false;
    }
    mm->page = page;
    mm->mapid = thread_current()->mapid;
    list_push_back(&thread_current()->mmap_list, &mm->elem);
    return true;
}

//pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/vm/mmap-unmap -a mmap-unmap -p ../../tests/vm/sample.txt -a sample.txt --swap-size=4 -- -q  -f run mmap-unmap

//pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/vm/mmap-overlap -a mmap-overlap -p tests/vm/zeros -a zeros --swap-size=4 -- -q  -f run mmap-overlap

void process_remove_mmap (int mapid)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->mmap_list);

  while(e != list_end(&t->mmap_list))
  {
    next = list_next(e);
    struct mmap_file *mm = list_entry(e, struct mmap_file, elem);
    if(mm->mapid == mapid || mapid==-1)
    {
      if(mm->page->physical_address != NULL)
      {
        struct page *p=mm->page;
        if(pagedir_is_dirty(thread_current()->pagedir,p->virtual_address))
        {             
          file_write_at (p->file,p->physical_address,p->read_bytes,p->offset);
        }
      }
      list_remove(&mm->elem);
      delete_page (&thread_current()->supp_page_table,mm->page);
      free(mm);
    }
      e = next;
  }
}
