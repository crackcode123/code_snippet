// string_pattern.cpp : Defines the entry point for the console application.

#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <algorithm>
#include <iomanip>

using namespace std;

/*
	compute table to have largest prefix length of the substring which is also suffix of same substring

	abab
	a       = 0
	ab      = 0
	aba     = 1
	abab    = 2

	abcabc
	a       = 0
	ab      = 0
	abc     = 0
	abca    = 1
	abcab   = 2
	abcabc  = 3

	abcaabc
	a       = 0
	ab      = 0
	abc     = 0
	abca    = 1
	abcaa   = 1
	abcaab  = 2
	abcabc  = 3

	ababdc
	a       = 0
	ab      = 0
	aba     = 1
	abab    = 2
	ababd   = 0
	ababdc  = 0
*/

static void compute_longest_suffix_prefix_table(std::vector<int>& table, const std::string& pattern)
{
	size_t length = pattern.length();
	table.resize(length);
	table[0] = 0;

	for (size_t k = 1; k < length; ++k)
	{
		size_t i = table[k - 1];
		while (1)
		{
			if (pattern[i] == pattern[k])
			{
				table[k] = i + 1;
				break;
			}
			if (i == 0)
			{
				table[k] = 0;
				break;
			}
			i = table[i];
		}
	}

	cout << "For pattern = " << pattern.c_str() << endl;
	size_t length_table = table.size();
	for (size_t x = 0; x < length_table; ++x)
	{
		cout << table[x] << endl;
	}
}

int main(int argc, char* argv[])
{
	vector<int> table;

	// [ Unit Test ] - 1   abab        -> 0 0 1 2
	// [ Unit Test ] - 2   abcabc      -> 0 0 0 1 2 3
	// [ Unit Test ] - 3   abcaabc     -> 0 0 0 1 1 2 3
	string pattern = "abcaabc";
	compute_longest_suffix_prefix_table(table, pattern);
	return 0;
}
