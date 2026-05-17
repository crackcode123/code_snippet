// graph_discovery.cpp : Defines the entry point for the console application.
//

#include<list>
#include<unordered_map>
#include<unordered_set>
#include<stack>
#include<iostream>
#include<queue>

class Graph
{
	int m_vertices;			// No. of vertices
	std::list<int> *adj;    // Pointer to an array containing adjacency lists
//
// control plane
//
public:
	Graph(int V);  // Constructor
	~Graph();
	void addEdge(int v, int w); // function to add an edge to graph
	static void build_graph();
	std::list<int>& get_adj_list(int index)
	{
		return adj[index];
	}
	int get_vertices_count()
	{
		return m_vertices;
	}

public :
//
// Data planes :
//
	static void create_graph(Graph& g);
	void toplogy_sort();
	void bfs(int index);
	void dfs(int index);
	void bidir_bfs();

	void minimum_spanning_tree();
	void shortest_path();
	void boggle();

	bool is_cyclic();
	void is_bipartite();
};
Graph::Graph(int v) :m_vertices(v)
{
	adj = new std::list<int>[v];
}
Graph::~Graph()
{
	delete[] adj;
}
void Graph::addEdge(int v, int w)
{
	adj[v - 1].push_back(w - 1);
}

void Graph::create_graph(Graph& g)
{
	g.addEdge(1, 2);
	g.addEdge(1, 3);
	g.addEdge(2, 3);
	g.addEdge(2, 5);
	g.addEdge(3, 4);
	g.addEdge(5, 4);

	for (int i = 0; i < g.get_vertices_count(); ++i)
	{
		std::list<int>& my_list = g.get_adj_list(i);
		std::list<int>::iterator it = my_list.begin();
		std::list<int>::iterator end = my_list.end();
		std::cout << (i+1) << "\t";
		for (; it != end; ++it)
		{
			std::cout << ((*it)+1) << "\t";
		}

		std::cout << "\n";
	}
}

/*
	Topological sorting
*/

static void toplogy_sort_util(Graph& g, int index, std::unordered_set < int>& my_set, std::stack<int>& my_stack)
{
	std::list<int>::const_iterator it = g.get_adj_list(index).begin();
	std::list<int>::const_iterator end = g.get_adj_list(index).end();
	my_set.insert(index);

	for (; it != end; ++it )
	{
		std::unordered_set < int>::iterator it_set = my_set.find(*it); // O(1) search
		std::unordered_set < int>::iterator end_set = my_set.end();

		if (it_set != end_set)
			continue;
		toplogy_sort_util(g, *it, my_set, my_stack);
	}
	my_stack.push(index);
}
void Graph::toplogy_sort()
{

	std::unordered_set <int> my_set;
	std::stack<int> my_stack;

	//
	// iterate through the graph
	//
	for (int i = 0; i < m_vertices; ++i)
	{
		std::unordered_set < int>::iterator it_set = my_set.find(i); // o(1) search
		std::unordered_set < int>::iterator end_set = my_set.end();
		if (it_set != end_set)
			continue;
		toplogy_sort_util(*this,i, my_set, my_stack);
	}
	int len = my_stack.size();
	for (int i = 0; i < len; ++i)
	{
		std::cout << my_stack.top()+1<<"\t";
		my_stack.pop();
	}
}
static void dfs_util(Graph& g, int index, std::unordered_set < int>& my_set)
{

	my_set.insert(index);
	std::cout << "node =" << index + 1 << "\n";
	std::list<int>::const_iterator it = g.get_adj_list(index).begin();
	std::list<int>::const_iterator end = g.get_adj_list(index).end();
	for (; it != end; ++it)
	{
		std::unordered_set < int>::iterator it_set = my_set.find(*it); // O(1) search
		std::unordered_set < int>::iterator end_set = my_set.end();

		if (it_set != end_set)
			continue;
		dfs_util(g, *it, my_set);
	}

}
void Graph::dfs(int index)
{
	std::unordered_set <int> my_set;
	dfs_util(*this, index - 1, my_set);
}

void Graph::bfs(int index /* 0 based*/)
{
	std::queue<int> my_queue;
	std::unordered_set <int> my_set;
	my_queue.push(index);
	my_set.insert(index);

	while (!my_queue.empty())
	{
		int item = my_queue.front();
		std::cout << "node =" << item + 1 << "\n";
		my_queue.pop();
		std::list<int>::const_iterator it = get_adj_list(item).begin();
		std::list<int>::const_iterator end = get_adj_list(item).end();
		for (; it != end; ++it)
		{
			std::unordered_set < int>::iterator it_set = my_set.find(*it); // O(1) search
			std::unordered_set < int>::iterator end_set = my_set.end();

			if (it_set != end_set)
				continue;
			my_set.insert(*it);
			my_queue.push(*it);
		}
	}
}

void Graph::minimum_spanning_tree()
{

}
void Graph::shortest_path()
{

}
void Graph::is_bipartite()
{

}

void Graph::bidir_bfs()
{

}

void Graph::boggle()
{

}

int main(int argc, char* argv[])
{
	Graph g(5);
	Graph::create_graph(g);
	//g.toplogy_sort();
	//g.dfs(2);
	g.bfs(1);
	return 0;
}

