#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#define USER_VADDR_BOTTOM ((void *) 0x08048000)

struct lock file_lock;//it makes multiple process cannot access to syscall which access file system
void syscall_init (void);
void sys_exit (int);
struct file *get_file (int);
#endif /* userprog/syscall.h */

int mmap(int fd, void *addr);
void munmap(int mapid);
