// Topological_Sorting.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include<vector>
#include<list>
#include<stack>

struct CGraph
{
public:
	CGraph(int v)
	{
		_the_graph.resize(v);
	}
	void add_edge(int from,int to)
	{
		std::list<int>& _list = _the_graph[from];
		_list.push_back(to);
	}

	void topology_sort()
	{
		std::stack<int> sorted_vertices;
		std::vector<bool> visited;
		size_t _size= _the_graph.size();
		visited.resize(_size);

		for (int k = 0; k < _size; ++k)
		{
			visited[k] = false;
		}
		for (int i = 0; i < _size; ++i)
		{
			if (!visited[i])
				topology_sort_utility(sorted_vertices, visited, i);
		}
	}


	typedef std::vector<std::list<int> > the_graph;

private:
	void topology_sort_utility(std::stack<int>& _stack,std::vector<bool>& visited, int v)
	{
		visited[v] = true;
		
		std::list<int>& _list = _the_graph[v];
		std::list<int>::iterator it = _list.begin();
		std::list<int>::iterator end = _list.end();
		for (; it != end; ++it)
		{
			if (!visited[*it])
			{
				topology_sort_utility(_stack, visited, (*it));
			}
		}
		_stack.push(v);

	}
	the_graph _the_graph;
};
#include<stdlib.h>
class Tiles2xN
{
public:
	Tiles2xN()
	{}
	size_t count_ways(size_t cols)
	{
		if (cols == 0)
			return 0;
		if (cols == 1)
			return 1;
		return count_ways(cols - 1) + 1 + count_ways(cols - 2);
	}

	size_t count_ways_other(size_t cols)
	{
		if (cols == 0)
			return 0;
		if (cols == 1)
			return 1;
		if (cols == 2)
			return 2;
		return count_ways(cols - 1) + count_ways(cols - 2);
	}
private:

};

int main()
{
	Tiles2xN x;
	size_t kk= x.count_ways(5);
	size_t pp = x.count_ways_other(5);

    return 0;
}

