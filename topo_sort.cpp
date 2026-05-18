// Topological_Sorting.cpp : Defines the entry point for the console application.
//

#include<iostream>
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

	void print_graph()
	{
		size_t _size= _the_graph.size();
		for (int i = 0; i< _size; ++i)
		{
			std::cout << "graph index = " << i << "=>  " ;
			std::list<int>& _list = _the_graph[i];
			for(auto y:_list)
			{
				std::cout << y << ",";
			}
			std::cout << std::endl;
		}
		
	}

	void print_topo_sort(std::stack<int>& sorted_vertices)
	{
		while (!sorted_vertices.empty())
  		{
      		std::cout << sorted_vertices.top() << '\t';
      		sorted_vertices.pop();
 		 }
	}
	void topology_sort()
	{
		std::stack<int> sorted_vertices;
		std::vector<bool> visited;
		size_t _size= _the_graph.size();
		std::cout << "graph size= " << _size <<std::endl;
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

		std::cout << std::endl << "sorted vector" << std::endl;

		print_topo_sort(sorted_vertices);
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

int main()
{
	CGraph cg(10);

	cg.add_edge(0,2);
	cg.add_edge(2,3);
	cg.add_edge(4,5);
	cg.add_edge(2,7);
	cg.add_edge(6,8);
	cg.add_edge(3,5);

	cg.print_graph();
	cg.topology_sort();

    return 0;
}

