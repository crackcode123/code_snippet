// hack_graph.cpp : Defines the entry point for the console application.
//

#include<list>
#include<vector>
#include<unordered_set>
#include<queue>

struct Node
{
	size_t node;
	Node(size_t _node) :node(node)
	{}
};
class Graph
{
public:
	Graph(size_t _vertices) :vertices(_vertices)
	{
		adj_list = new std::list<Node*>[vertices];
	}
	void addNode(size_t u, size_t v)
	{
		Node* my_node = new Node(v);
		adj_list[u].push_back(my_node);
	}

	std::list<Node*>& get_adj_list()
	{
		return *adj_list;
	}

	bool bfs_search(Node* src, Node* dest);

	bool is_loop();
	bool is_bipartite();

	~Graph()
	{
		delete[] adj_list;
	}

private:
	size_t vertices;
	std::list<Node*> *adj_list;
};

bool Graph::bfs_search(Node* src, Node* dest)
{
	if (src->node == dest->node)
		return true;

	std::queue<Node*> my_queue;
	std::unordered_set<Node*> visited_set;
	visited_set.insert(src);
	my_queue.push(src);

	while (!my_queue.empty())
	{
		Node* front_node = my_queue.front();
		my_queue.pop();
		visited_set.insert(front_node);

		std::list<Node*>& srcNodelist = adj_list[src->node];
		for (auto x : srcNodelist)
		{
			std::unordered_set<Node*>::iterator it = visited_set.find(x);
			std::unordered_set<Node*>::iterator end = visited_set.end();
			if (it != end)
				continue;
			if ((*it)->node == dest->node)
				return true;
			my_queue.push(*it);
		}
	}
	return true;
}

static bool is_loop_utility(Graph& gp, Node& parent, std::vector<bool>& in_stack, std::vector<bool>& visited)
{
	if (!visited[parent.node])
	{
		in_stack[parent.node] = true;
		visited[parent.node] = true;

		std::list<Node*>& mlist = gp.get_adj_list();

		for (auto v : mlist)
		{
			if (!visited[v->node] && is_loop_utility(gp, *v, in_stack, visited))
			{
				return true;
			}
			else
			{
				if (in_stack[parent.node])
					return true;
			}
		}
	}
	in_stack[parent.node] = false;
	return false;
}

bool Graph::is_loop()
{
	std::vector<bool> in_stack;
	in_stack.resize(vertices);

	std::vector<bool> visited;
	visited.resize(vertices);

	for (auto v : *adj_list)
	{
		bool loop = is_loop_utility(*this, *v, in_stack, visited);
		if (loop == true)
			return true;
	}
	return false;
}
bool Graph::is_bipartite()
{
	return true;
}

int main()
{
    return 0;
}

