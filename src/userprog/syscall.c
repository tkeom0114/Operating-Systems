#include "userprog/syscall.h"
#include "userprog/process.h"  //added at 10/10 23:05
#include "userprog/pagedir.h"  //added at 11/13 01:57
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "devices/input.h"
#include "lib/user/syscall.h"   //added at 10/10 23:05
#include "lib/kernel/stdio.h"  //added at 10/29 23:21
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"     //added at 10/10 23:05
#include "threads/synch.h"     //added at 10/10 23:05
#include "devices/shutdown.h"  //added at 10/10 23:05
#include "filesys/filesys.h"   //added at 10/10 23:05
#include "filesys/file.h"      //added at 10/30 16:24

#ifdef VM
  #include "vm/page.h"
#endif

static void syscall_handler (struct intr_frame *);


void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

/*Checking the address is in user space.
Made by Taekang Eom
Time: 10/10 12:33 */
#ifndef VM
void check_address (void *ptr, void *esp UNUSED)
{
  if(!is_user_vaddr(ptr) || ptr < (void *)0x08048000)
  {
    sys_exit(-1);
  }

}
#else
/*Checking the address is in user space.
Made by Taekang Eom
Time: 11/29 20:34 */
struct page* check_address (void *ptr, void *esp)//fixed at 11/28 13:02
{
  struct page *p = find_page(&thread_current ()->supp_page_table,ptr);
  if(p == NULL)
  {
    p = grow_stack (ptr,esp);
    if (p == NULL)
      sys_exit (-1);
  }
  return p;
}

/*Checking the address of buffer.
Made by Taekang Eom
Time: 11/29 20:34 */
void check_buffer(void* buffer, unsigned size, void* esp, bool to_write)
{
  unsigned cur_buffer = (unsigned) buffer;
  int cur_size = (int)size;
  struct page *p;
  while (cur_size > 0)
  {
    p = check_address ((void*)cur_buffer,esp);

    if (!p->writable && to_write)
      sys_exit (-1);
    cur_buffer += PGSIZE;
    cur_size -= PGSIZE;
  }
  p = check_address (buffer + size,esp);
  if (!p->writable && to_write)
      sys_exit (-1);
  return ;
}

/*Checking the address of string.
Made by Taekang Eom
Time: 11/29 20:34 */
void check_string(const char *str, void* esp)
{
  unsigned cur = (unsigned) str;
  int cur_length = (int)strlen(str);
  struct page *p;
  while (cur_length > 0)
  {
    p = check_address ((void*)cur,esp);
    cur += PGSIZE;
    cur_length -= PGSIZE;
  }
  p = check_address (str + strlen (str), esp);
  return ;
}

#endif


/*Get arguments from user stack.
Made by Taekang Eom
Time: 10/10 12:43 */
void save_argument (void *esp,int *arg,int count)
{
  int i;
  int *ptr;
  for (i=0;i<count;i++)
  {
    ptr = (int*)esp+i+1;
    check_address (ptr,esp);
    arg[i] = *ptr;
  }
}

/*Find file which file desciptor is fd.
Made by Taekang Eom
Time: 10/30 16:42 */
struct file *get_file (int fd)
{
  if(fd<0 || fd>128)
    return NULL;
  return thread_current () -> file_table[fd];
}

/*Shutdown pintos.
Made by Taekang Eom
Time: 10/10 13:43 */
void sys_halt (void)
{
  shutdown_power_off ();
}

/*exit current thread.
Made by Taekang Eom
Time: 10/10 14:07 */
void sys_exit (int status)
{
  struct thread *t = thread_current ();
  t->exit_status = status;
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit ();
}

/*Create and execute child process.
Made by Taekang Eom
Time: 10/10 23:01 */
pid_t sys_exec (const char *file,void *esp)
{
  #ifndef VM
  check_address (file,esp);
  check_address (file + strlen (file),esp);
  #else
  check_string (file,esp);
  #endif
  lock_acquire (&file_lock);
  tid_t tid = process_execute (file);
  lock_release (&file_lock);
  return tid;
}

/*Wait child processes until terminated.
Made by Taekang Eom
Time: 10/10 23:01 */
int sys_wait (pid_t pid)
{
  return process_wait (pid);
}

/*Create File.
Mate by Taekang Eom
Time: 10/10 15:45 */
bool sys_create (const char *file , unsigned initial_size, void *esp)
{
  #ifndef VM
  check_address (file,esp);
  check_address (file + strlen (file),esp);
  #else
  check_string (file,esp);
  #endif
  if (file == NULL)
    sys_exit (-1);
  lock_acquire (&file_lock);
  bool success = filesys_create (file,initial_size);
  lock_release (&file_lock);
  return success;
}

/*Remove File.
Mate by Taekang Eom
Time: 10/10 15:45 */
bool sys_remove (const char *file, void *esp)
{
  #ifndef VM
  check_address (file,esp);
  check_address (file + strlen (file),esp);
  #else
  check_string (file,esp);
  #endif
  if (file == NULL)
    sys_exit (-1);
  lock_acquire (&file_lock);
  bool success;
  success = filesys_remove (file);
  lock_release (&file_lock);
  return success;
}

/*oepn file
Made by Taekang Eom
Time: 10/31 14:31*/
int sys_open (const char *file, void *esp)
{
  #ifndef VM
  check_address (file,esp);
  check_address (file + strlen (file),esp);
  #else
  check_string (file,esp);
  #endif
  if(file == NULL)
    sys_exit (-1);
  lock_acquire (&file_lock);
  int fd;
  struct thread *cur = thread_current ();
  struct file *opening_file = filesys_open (file);
  if (opening_file == NULL)
    fd = -1;
  else
  {
    fd = cur->next_fd;
    cur->next_fd++;
    cur->file_table[fd] = opening_file;
  }
  lock_release (&file_lock);
  return fd;
}

/* Returns the size of FILE in bytes.
Made by Taekang Eom
Time: 11/02 04:49 */
int sys_filesize (int fd)
{
  int return_size;
  lock_acquire (&file_lock);
  struct file *size_file = get_file (fd);
  if (size_file == NULL)
    return_size = -1;
  else
    return_size = file_length (size_file);
  lock_release (&file_lock);
  return return_size;
}



/*Read from open file to buffer
Made by Taekang Eom
Time: 10/29 23:17*/
int sys_read (int fd, void *buffer, unsigned size, void *esp)
{
  #ifndef VM
  check_address (buffer,esp);
  check_address (buffer+size,esp);
  #else
  check_buffer (buffer,size,esp,true);
  #endif
  lock_acquire (&file_lock);
  int return_size;
  if (fd == 0)
  {
    return_size = input_getc ();
  }
  else if (fd == 1)
    return_size = -1;
  else
  {
    struct file *reading_file = get_file (fd);
    if (reading_file == NULL)
      return_size = -1;
    else
      return_size = file_read (reading_file,buffer,size);
  }

  lock_release (&file_lock);
  return return_size;
}

/*Write from buffer to open file
Made by Taekang Eom
Time: 10/29 23:17*/
int sys_write (int fd, void *buffer, unsigned size,void *esp)
{
  #ifndef VM
  check_address (buffer,esp);
  check_address (buffer+size,esp);
  #else
  check_buffer (buffer,size,esp,false);
  #endif
  lock_acquire (&file_lock);
  int return_size;
  if (fd == 1)
  {

    putbuf (buffer,size);
    return_size = size;
  }
  else if (fd == 0)
    return_size = -1;
  else
  {
    struct file *writing_file = get_file (fd);
    if (writing_file == NULL)
      return_size = -1;
    else
      return_size = file_write (writing_file,buffer,size);
  }
  lock_release (&file_lock);
  return return_size;
}

/*close file
Made by Taekang Eom
Time: 10/31 14:31*/
void sys_close (int fd)
{
  lock_acquire (&file_lock);
  struct thread *cur = thread_current ();
  struct file *closing_file = get_file (fd);
  if (closing_file == NULL)
  {
    lock_release (&file_lock);
    sys_exit (-1);
  }
  file_close (closing_file);
  thread_current ()->file_table[fd] = NULL;
  lock_release (&file_lock);
}

/* Sets the current position in FILE to NEW_POS bytes from the
   start of the file.
   Made by Taekang Eom
   Time: 11/02 03:23*/
void sys_seek (int fd, unsigned position)
{
  lock_acquire (&file_lock);
  struct file *seek_file = get_file (fd);
  if(seek_file == NULL)
  {
    lock_release (&file_lock);
    sys_exit (-1);
  }
  file_seek (seek_file,position);
  lock_release (&file_lock);
}

/* Returns the current position in FILE as a byte offset from the
   start of the file.
   Made by Taekang Eom
   Time: 11/02 03:24*/
unsigned sys_tell (int fd)
{
  lock_acquire (&file_lock);
  unsigned tell;
  struct file *tell_file = get_file (fd);
  if(tell_file == NULL)
    tell = -1;
  tell = file_tell (tell_file);
  lock_release (&file_lock);
  return tell;
}

/*Fixed by Taekang Eom
Time: 10/10 12:48 */
static void
syscall_handler (struct intr_frame *f)
{
  check_address(f->esp,f->esp);
  int arg[3];
  int syscall_num = *(int*)f->esp;
  switch(syscall_num)
  {
    case SYS_HALT:
      sys_halt ();
      break;
    case SYS_EXIT:
      save_argument (f->esp,arg,1);
      sys_exit (arg[0]);
      break;
    case SYS_EXEC:
      save_argument (f->esp,arg,1);
      f->eax = sys_exec (arg[0],f->esp);
      break;
    case SYS_WAIT:
      save_argument (f->esp,arg,1);
      f->eax = sys_wait (arg[0]);
      break;
    case SYS_CREATE:
      save_argument (f->esp,arg,2);
      f->eax = sys_create (arg[0],arg[1],f->esp);
      break;
    case SYS_REMOVE:
      save_argument (f->esp,arg,1);
      f->eax = sys_remove (arg[0],f->esp);
      break;
    case SYS_OPEN:
      save_argument (f->esp,arg,1);
      f->eax = sys_open (arg[0],f->esp);
      break;
    case SYS_FILESIZE:
      save_argument (f->esp,arg,1);
      f->eax = sys_filesize (arg[0]);
      break;
    case SYS_READ:
      save_argument (f->esp,arg,3);
      f->eax = sys_read (arg[0],arg[1],arg[2],f->esp);
      break;
    case SYS_WRITE:
      save_argument (f->esp,arg,3);
      f->eax = sys_write (arg[0],arg[1],arg[2],f->esp);
      break;
    case SYS_SEEK:
      save_argument (f->esp,arg,2);
      sys_seek (arg[0],arg[1]);
      break;
    case SYS_TELL:
      save_argument (f->esp,arg,1);
      f->eax = sys_tell (arg[0]);
      break;
    case SYS_CLOSE:
      save_argument (f->esp,arg,1);
      sys_close (arg[0]);
      break;
  }
}
int mmap (int fd, void *addr)
{
  struct file *_file = get_file(fd);
  if (!_file || !is_user_vaddr(addr) || addr < USER_VADDR_BOTTOM ||
      ((uint32_t) addr % PGSIZE) != 0)
    {
      return ERROR;
    }
  struct file *file = file_reopen(_file);
  if (!file || file_length(old_file) == 0)
    {
      return ERROR;
    }
  thread_current()->mapid++;
  int32_t ofs = 0;
  uint32_t read_bytes = file_length(file);
  while (read_bytes > 0)
    {
      uint32_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      uint32_t page_zero_bytes = PGSIZE - page_read_bytes;
      if (!add_mmap_to_page_table(file, ofs,
				  addr, page_read_bytes, page_zero_bytes))
	{
	  munmap(thread_current()->mapid);
	  return ERROR;
	}
      read_bytes -= page_read_bytes;
      ofs += page_read_bytes;
      addr += PGSIZE;
  }
  return thread_current()->mapid;
}


