// stream_heap_10.cpp : Defines the entry point for the console application.
//

// A C++ program to demonstrate common Binary Heap Operations
#include<iostream>
#include<climits>
using namespace std;

// Prototype of a utility function to swap two integers
void swap(int &x, int &y)
{
	x = x ^ y;
	y = x ^ y;
	x = x ^ y;
}

// A class for Min Heap
class MinHeap
{
	int *harr; // pointer to array of elements in heap
	int capacity; // maximum possible size of min heap
	int heap_size; // Current number of elements in min heap
public:
	// Constructor
	MinHeap(int capacity);

	// Destructor
	~MinHeap();

	// to heapify a subtree with root at given index
	void MinHeapify(int);

	int parent(int child_index);

	// to get index of left child of node at index i
	int left(int left_index);

	// to get index of right child of node at index i
	int right(int right_index);

	// to extract the root which is the minimum element
	int extractMin();

	// Decreases key value of key at index i to new_val
	void decreaseKey(int i, int new_val);

	// Returns the minimum key (key at root) from min heap
	int getMin() { return harr[0]; }

	// Deletes a key stored at index i
	void deleteKey(int i);

	// Inserts a new key 'k'
	void insertKey(int k);
};

MinHeap::MinHeap(int capcity)
{
	harr = new int[capacity];
	this->capacity = capacity;
	heap_size = 0;
}

MinHeap::~MinHeap()
{
	delete[] harr;
}

void MinHeap::MinHeapify(int root)
{

}
int MinHeap::parent(int i)
{
	//
	// 0 based
	//
	return (i - 1) / 2;
}

void MinHeap::insertKey(int value) // logn
{
	if (heap_size == capacity)
		return;

	int prev_pos = heap_size;
	harr[heap_size] = value;
	heap_size++;

	//
	// now go up to adjust parent
	//

	int parent_x = parent(prev_pos);
	while (harr[parent_x] > value)
	{
		swap(value, harr[parent_x]);
		parent_x = parent(parent_x);
	}

}

// to get index of left child of node at index i
int MinHeap::left(int i)
{
	//
	// 0 based index
	//
	return ((2 * i) + 1);
}

// to get index of right child of node at index i
int MinHeap::right(int i)
{
	//
	// 0 based index
	//
	return (left(i)+1);
}

#include<stdio.h>

int main(int argc, char** argv)
{
	int x = 3, y = 5;
	swap(x, y);
	printf("x = %d, y= %d", x, y);
	return 0;
}

