#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H



void syscall_init (void);
void sys_close (int);
void sys_exit (int);
struct file *get_file (int);
#endif /* userprog/syscall.h */
