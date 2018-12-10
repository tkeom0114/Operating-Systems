/*
Author: Taekang Eom
Time: 11/27 15:54
*/
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/file.h"
#include "filesys/filesys.h"
#include <list.h>
#include <hash.h>

enum page_type
{
	EXE_PAGE=0,
	SWAP_PAGE,
    MMAP_PAGE
};

struct page
{
	uint8_t type;//page의 타입(FILE에서 로드 된 것인지, Binary page인지)
	void *virtual_address;//virtual page가 시작되는 위치
	void *physical_address;//physical frame이 시작되는 위치
	bool writable;//쓰기 가능 여부를 나타냄
	struct file *file;//해당 페이지가 쓰는 file을 나타냄
	size_t offset;//해당 page에서 읽거나, 쓰고있는 곳
	size_t read_bytes;//페이지에서 실제로 데이터가 쓰인 byte의 수
	size_t zero_bytes;//0으로 채워야 할 byte의 수
	struct hash_elem page_elem;//hash element로 사용함
	struct list_elem frame_elem;//frame table에 사용됨
	struct list_elem mmap_list_elem;//mmap_file의 page_list에 사용됨
	size_t swap_slot;//disk로 swap된 경우 어느 slot에 있는지 알려줌
};
struct list frame_list;//frame table
struct list_elem *clock;//clock algorithm에서 가리키는 page의 list_elem
struct bitmap *swap_table;//swap_table
struct block *swap_block;
size_t swap_size;

void page_table_init (struct hash *supp_page_table);
bool insert_page (struct hash *supp_page_table, struct page *p);
bool delete_page (struct hash *supp_page_table, struct page *p);
struct page *find_page (struct hash *supp_page_table, void *virtual_address);
void page_destroy_func (struct hash_elem*e, void *aux);
void destroy_page_table (struct hash *supp_page_table);
struct page* grow_stack (void *ptr, void *esp);
uint8_t *evict_page ();

bool add_mmap_to_page_table(struct file *file, int32_t offset, uint8_t *upage,
        uint32_t read_bytes, uint32_t zero_bytes);
#endif
