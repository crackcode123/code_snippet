#include<algorithm>
#include<iostream>
#include <functional>

// Data structure to store a Binary Search Tree node
struct Node {
	int data;
	Node *left, *right;
};

// Function to create a new binary tree node having given key
Node* newNode(int key)
{
	Node* node = new Node;
	node->data = key;
	node->left = node->right = nullptr;

	return node;
}

// Function to perform in-order traversal of the tree
void inorder(Node* root)
{
	if (root == nullptr)
		return;

	inorder(root->left);
	std::cout << root->data << " ";
	inorder(root->right);
}

// Recursive function to insert an key into BST
Node* insert(Node* root, int key)
{
	// if the root is null, create a new node an return it
	if (root == nullptr)
		return newNode(key);

	// if given key is less than the root node, recurse for left subtree
	if (key < root->data)
		root->left = insert(root->left, key);

	// if given key is more than the root node, recurse for right subtree
	else
		root->right = insert(root->right, key);

	return root;
}

#include<list>
/* BST Sequences */
static void array_subsequence_util(std::list<int>& first, std::list<int>& second, std::vector <std::list<int>>& results, std::list<int>& prefix)
{
	if (first.size() == 0 || second.size() == 0)
	{
		std::list<int> result(prefix);
		if (first.size() == 0)
		{
			for (auto a : second)
			{
				result.push_back(a++);
			}
		}

		if (second.size() == 0)
		{
			for (auto a : first)
			{
				result.push_back(a++);
			}
		}
		results.push_back(result);
		return;
	}

	int first_int = first.front();
	first.pop_front();
	prefix.push_back(first_int);
	array_subsequence_util(first, second, results, prefix);
	/* back track*/
	first.push_front(first_int);
	prefix.pop_back();

	int second_int = second.front();
	second.pop_front();
	prefix.push_back(second_int);
	array_subsequence_util(first, second, results, prefix);
	/* back track*/
	second.push_front(second_int);
	prefix.pop_back();
}

void array_subsequence(std::list<int>& first, std::list<int>& second, std::vector< std::list<int>>& results)
{
	std::list<int> prefix;
	array_subsequence_util(first, second, results, prefix);
}

int main()
{
	std::list<int> first = { 1,2 };
	std::list<int> second = { 3,4 };

	std::vector< std::list<int>> results;
	array_subsequence(first, second, results);

}
