#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


struct lock file_lock;//added at 10/10 16:20
void syscall_init (void);
void sys_close (int);
void sys_exit (int);
struct file *get_file (int);
#endif /* userprog/syscall.h */
