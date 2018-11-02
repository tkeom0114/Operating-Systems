#include "userprog/syscall.h"
#include "userprog/process.h"  //added at 10/10 23:05
#include <stdio.h>
#include <syscall-nr.h>    
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

static void syscall_handler (struct intr_frame *);
struct lock file_lock;//added at 10/10 16:20
//pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/write-bad-ptr -a write-bad-ptr -p ../../tests/userprog/sample.txt -a sample.txt -- -q  -f run write-bad-ptr
//pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/exec-missing -a exec-missing -- -q  -f run exec-missing
//pintos -v -k -T 360 --qemu  --filesys-size=2 -p tests/userprog/no-vm/multi-oom -a multi-oom -- -q  -f run multi-oom
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

/*Checking the adress is in user space.
Made by Taekang Eom
Time: 10/10 12:33 */
void check_adress (void *ptr)
{
  if(!is_user_vaddr(ptr) || ptr < (void *)0x08048000)
    sys_exit(-1);
}

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
    check_adress (ptr);
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
  return thread_current ()->file_table[fd];
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
  printf("%s: exit(%d)\n",t->name,status);
  //while (true) {;}//debugging
  thread_exit ();
}

/*Create and execute child process.
Made by Taekang Eom
Time: 10/10 23:01 */
pid_t sys_exec (const *file)
{
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
bool sys_create (const char *file , unsigned initial_size)
{
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
bool sys_remove (const char *file)
{
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
int sys_open (const char *file)
{
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
int sys_read (int fd, void *buffer, unsigned size)
{
  check_adress (buffer);
  check_adress (buffer+size);
  lock_acquire (&file_lock);
  int return_size;
  if (fd == 0)
    return_size = input_getc ();
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
int sys_write (int fd, void *buffer, unsigned size)
{
  check_adress (buffer);
  check_adress (buffer+size);
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
  struct file *tell_file = get_file (fd);
  if(tell_file == NULL)
  {
    lock_release (&file_lock);
    return -1;
  }
  int tell = file_tell (tell_file);
  lock_release (&file_lock);
}

/*Fixed by Taekang Eom
Time: 10/10 12:48 */
static void
syscall_handler (struct intr_frame *f) 
{
  //printf ("system call!\n");
  //thread_exit ();
  check_adress(f->esp);
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
      f->eax = sys_exec (arg[0]);
      break;
    case SYS_WAIT:
      save_argument (f->esp,arg,1);
      f->eax = sys_wait (arg[0]);
      break;
    case SYS_CREATE: 
      save_argument (f->esp,arg,2);
      f->eax = sys_create (arg[0],arg[1]);
      break;
    case SYS_REMOVE:
      save_argument (f->esp,arg,1);
      f->eax = sys_remove (arg[0]);  
      break;            
    case SYS_OPEN:
      save_argument (f->esp,arg,1);
      f->eax = sys_open (arg[0]);
      break;             
    case SYS_FILESIZE:
      save_argument (f->esp,arg,1);
      f->eax = sys_filesize (arg[0]);
      break;         
    case SYS_READ:
      save_argument (f->esp,arg,3);
      f->eax = sys_read (arg[0],arg[1],arg[2]);
      break;             
    case SYS_WRITE: 
      save_argument (f->esp,arg,3);
      f->eax = sys_write (arg[0],arg[1],arg[2]);
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