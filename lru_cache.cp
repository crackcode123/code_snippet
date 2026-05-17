// hash_consistent.cpp : Defines the entry point for the console application.
//


#include<stdlib.h>
#include<string>
#include<stdint.h>
#include<stddef.h>


//
//  LRU data structure 
//

//
//	we need a double linked list and hash table to store page no as key and location of file in queue if found 
//	let say initial cacje size = 5
//  total pages = 50 .... started from 1 ..... 50
//

#define cacahe_size 5
#define total_page_size 50

//
// Page structure
//
struct Page
{
	uint16_t page_no;
	void* page_content;
};

//
// DQueue element
//
struct DQueueElem
{
	Page* _this_page;
	DQueueElem* next;
	DQueueElem* prev;
};

//
struct bin_hash
{	
	uint64_t address;
};

//
//  hash table is now implemented as array - can be enhanced 
//
bin_hash this_bin_hash[total_page_size];

//
// global DQueueElem - can be enhanced ( abstarcted )
//
DQueueElem *head, *tail;

static DQueueElem* create_element()
{
	DQueueElem* elem = (struct DQueueElem*)malloc(sizeof(DQueueElem));
	elem->prev = 0;
	elem->next = 0;
	elem->_this_page = 0;
	return elem;
}

static void init_cache()
{
	DQueueElem* first_elem = create_element();
	head = tail = first_elem;

	for (int i = 0; i < (cacahe_size-1); ++i)
	{
		DQueueElem* elem = create_element();
		tail->next = elem;
		elem->prev = tail;
		tail = elem;
	}

	for (int j = 0; j < total_page_size; ++j)
	{
		this_bin_hash[j].address = 0;
	}
}

static uint64_t page_address(struct Page* this_page)
{
	return this_bin_hash[(this_page->page_no) - 1].address;
}


static void adjust_queue_elem(struct DQueueElem* recent_element, bool is_tail)
{
	struct DQueueElem* tmp = recent_element->prev;
	tmp->next = recent_element->next;
	recent_element->next = head;
	recent_element->prev = head->prev;
	head->prev = recent_element;
	head = recent_element;
	
	if (tail)
		tail = tmp;
}

static void bring_recent_elem_into_front(struct DQueueElem* recent_element)
{
	// if the current element is head nothing to do
	if (head == recent_element) return;

	// if recent_element is last element then update tail pointer

	adjust_queue_elem(recent_element, tail == recent_element);
}

static void update_hash_bin(struct Page* new_page, uint64_t new_address, struct Page* old_page)
{
	this_bin_hash[(new_page->page_no) - 1].address = new_address;
	if (old_page && ((old_page->page_no) > 0) && ((old_page->page_no) < total_page_size))
		this_bin_hash[(old_page->page_no) - 1].address = 0;
}

static void enqueue_page(struct Page* this_page)
{
	struct Page* old = tail->_this_page;
	tail->_this_page = this_page;
	adjust_queue_elem(tail, true);
	update_hash_bin(this_page, (uint64_t)head, old);
}



static void ref_page(struct Page* page)
{
	//
	//  first look into hash table if page is already added 
	//

	uint64_t page_addr = page_address(page);

	// if page is available in the hash call bring_recent_elem_into_front_queue() and return to caller

	if (page_addr != 0)
	{
		DQueueElem* recent_elem = (DQueueElem*)page_addr;
		bring_recent_elem_into_front(recent_elem);
	}
	//
	// if page is not available 
	// enqueue the page 
	// update the hash table
	// return the object to caller
	//
	else
	{
		enqueue_page(page);
	}
	

}


static void dump_data()
{
	printf("\n");
	printf("head = %0x tail =%0x\n", head, tail);

	DQueueElem* current = head;
	for (int i = 0; i < 5; ++i)
	{
		printf("current = %0x , prev = %0x next =%0x\n", current, current->prev, current->next);
		current = current->next;
	}
}


static void LRU_cache_main()
{
	init_cache();
	struct Page* page_1 = (struct Page*)malloc(sizeof(Page));
	page_1->page_no = 1;

	struct Page* page_2 = (struct Page*)malloc(sizeof(Page));
	page_2->page_no = 2;

	struct Page* page_3 = (struct Page*)malloc(sizeof(Page));
	page_3->page_no = 3;

	struct Page* page_4 = (struct Page*)malloc(sizeof(Page));
	page_4->page_no = 4;

	struct Page* page_5 = (struct Page*)malloc(sizeof(Page));
	page_5->page_no = 5;

	struct Page* page_6 = (struct Page*)malloc(sizeof(Page));
	page_6->page_no = 6;

	struct Page* page_7 = (struct Page*)malloc(sizeof(Page));
	page_7->page_no = 7;

	struct Page* page_8 = (struct Page*)malloc(sizeof(Page));
	page_8->page_no = 8;

	dump_data();
	ref_page(page_1);
	dump_data();
	ref_page(page_1);
	dump_data();
	ref_page(page_2);
	dump_data();
	ref_page(page_3);
	dump_data();
	ref_page(page_3);
	dump_data();
	ref_page(page_4);
	dump_data();
	ref_page(page_5);
	dump_data();
	ref_page(page_1);
	dump_data();
	ref_page(page_6);
	dump_data();

	free(page_1);
	free(page_2);
	free(page_3);
	free(page_4);
	free(page_5);
	free(page_6);
	free(page_7);
	free(page_8);


}




