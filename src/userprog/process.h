#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
struct process_file {
    int fd;
    struct file *file;
    struct list_elem elem;
};

struct mmap_file// table of file mappings에 사용됨
{
	struct hash_elem mmap_elem;//hash element로 사용
	struct list page_list;//해당 파일이 저장된 page들의 list
	int mapid;//mmap을 통해 할당받은 mapid
	struct file *file;//mapping한 file을 저장
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
