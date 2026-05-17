// SalesTax.cpp : Defines the entry point for the console application.
//

/**************************************/

#include<unordered_map> // for hash table
#include<iostream>
#include<assert.h>

#define _TEST_

#ifdef _TEST_
#define CACHE_SIZE 3
#else
#define CACHE_SIZE 50000
#endif

class clru;

namespace utility
{
	static unsigned int generate_number_from_string(const std::string& address)
	{
		const int MULT = 31;
		unsigned int h = 0;
		const char* p = address.c_str();
		for (; *p; ++p)
		{
			h = (MULT * h) + *p;
		}
		return h;
	}

}

namespace sales_tax
{
	static double sales_tax_lookup(const std::string& address)
	{
		double sales_tax[] = {
			2.72, 1.91, 2.46, 2.03, 1.12, 2.96, 3.79, 1.46, 2.66, 3.72 };
		unsigned int h = utility::generate_number_from_string(address) % 10;  // just to simulate actual look up takes time
		return sales_tax[h];
	}

}

struct page
{
	double sales_tax;
	std::string addr;
};

template<class X>

struct dqueue_elem
{
	X _this_page;
	dqueue_elem* next;
	dqueue_elem* prev;
};

class clru // Least Recently Used
{
public:
	clru():_size(0)
	{
		dqueue_elem<page>* first_elem = create_element();
		_head = _tail = first_elem;

		//
		// pre allocated LRU cache
		//
		for (int i = 0; i < (CACHE_SIZE-1); ++i)
		{
			dqueue_elem<page>* elem = create_element();
			_tail->next = elem;
			elem->prev = _tail;
			_tail = elem;
		}
	}
	~clru()
	{

		for (int i = 0; i < CACHE_SIZE; ++i)
		{
			dqueue_elem<page>* tmp = _head;
			_head = _head->next;
			delete(tmp);
		}
	}

	double fast_rate_lookup(const std::string& address)
	{
		std::unordered_map<std::string, dqueue_elem<page>*>::iterator it = hash_table.find(address);
		std::unordered_map<std::string, dqueue_elem<page>*>::iterator end = hash_table.end();
		double sales_tax = 0.0;
		if (it != end)
		{
			dqueue_elem<page>* recent_elem = it->second;
			adjust_recent_elem_into_front(recent_elem);
			sales_tax = recent_elem->_this_page.sales_tax;
		}
		else
		{
			sales_tax = sales_tax::sales_tax_lookup(address);
			page my_page;
			my_page.addr = address;
			my_page.sales_tax = sales_tax;
			enqueue(my_page);
		}

		return sales_tax;
	}

	// utility function to dump cache - debug!!
	void dump_cache()
	{
		dqueue_elem<page>* my_haed = _head;

		for (; my_haed; my_haed = my_haed->next)
		{
			std::cout << my_haed->_this_page.addr.c_str() << std::endl;
		}
	}

private:
	dqueue_elem<page>* create_element()
	{
		dqueue_elem<page>* elem = new dqueue_elem<page>();
		elem->prev = 0;
		elem->next = 0;
		elem->_this_page.sales_tax = 0;
		return elem;
	}

	void adjust_recent_elem_into_front(dqueue_elem<page>* recent_element)
	{
		// if the current element is head nothing to do
		if (_head == recent_element) return;

		struct dqueue_elem<page>* tmp = recent_element->prev;
		tmp->next = recent_element->next;
		recent_element->next = _head;
		recent_element->prev = _head->prev;
		_head->prev = recent_element;
		_head = recent_element;

		// if the current element is tail assign _tail to tmp
		if (_tail == recent_element)
			_tail = tmp;
	}

	void enqueue(const page& my_page)
	{
		std::string& my_string = _tail->_this_page.addr;
		std::unordered_map<std::string, dqueue_elem<page>*>::iterator it = hash_table.find(my_string);
		std::unordered_map<std::string, dqueue_elem<page>*>::iterator end = hash_table.end();

		if (it != end )
		{
			assert(_size == CACHE_SIZE);
			hash_table.erase(my_string);
		}
		_tail->_this_page = my_page;
		hash_table.insert({ my_page.addr,_tail });
		adjust_recent_elem_into_front(_tail);
		if (_size < CACHE_SIZE)
			++_size;
	}

private:

	size_t _size;
	//
	// pre-allocated double link list for LRU caching
	//
	dqueue_elem<page>* _head;
	dqueue_elem<page>* _tail;

	//
	// Hash table - we can implement more efficient/pre-allocated hash table if needed
	//
	std::unordered_map<std::string, dqueue_elem<page>*> hash_table;
};

namespace sales_tax
{
	static double fast_rate_lookup(const std::string& address,clru& my_obj)
	{
		return my_obj.fast_rate_lookup(address);
	}
}
namespace test
{
	void test_lru_3()
	{
		clru my_obj;
		std::string my_array[3] = { "Robotics","Genetics","Nano" }; // , "CompuVision"

		for (int i = 0; i < CACHE_SIZE; ++i)
		{
			std::cout << sales_tax::fast_rate_lookup(my_array[i], my_obj) << std::endl;
		}
		my_obj.dump_cache(); //first element = "Nano"

		std::cout << sales_tax::fast_rate_lookup(my_array[1], my_obj); // must be from cache;
		my_obj.dump_cache(); // first element = "Genetics"
	}
	void test_lru_4()
	{
		clru my_obj;
		std::string my_array[CACHE_SIZE+1] = { "Robotics","Genetics","Nano" , "CompuVision" };

		for (int i = 0; i < CACHE_SIZE; ++i)
		{
			std::cout << sales_tax::fast_rate_lookup(my_array[i], my_obj) << std::endl;
			my_obj.dump_cache();
		}
		my_obj.dump_cache();
		for (int i = 0; i < (CACHE_SIZE+1); ++i)
		{
			std::cout << sales_tax::fast_rate_lookup(my_array[i], my_obj) << std::endl;
			my_obj.dump_cache();

		}
		// print cache - Robotics should be replaced with CompuVision in the cache;
		my_obj.dump_cache();

		sales_tax::fast_rate_lookup(my_array[0], my_obj);
		my_obj.dump_cache();
	}

}

int main()
{
	test::test_lru_3();
	test::test_lru_4();
    return 0;
}

