#include<vector>
#include<queue>
#include<algorithm>
#include<iostream>
#include <functional>

// function to calculate med of stream 
void CalculateMedians(double arr[], int n)
{
	std::priority_queue<double> min_heap;
	std::priority_queue<double, std::vector<double>, std::greater<double> > max_heap;

	double med = arr[0];
	min_heap.push(arr[0]);

	for (int i = 1; i < n; ++i)
	{
		double val = arr[i];
		if (min_heap.size() < max_heap.size())
		{
			if (val < med)
			{
				min_heap.push(val);
			}
			else
			{
				min_heap.push(max_heap.top());
				max_heap.pop();
				max_heap.push(val);
			}
			med = (max_heap.top() + min_heap.top()) / 2;

		}
		else if (max_heap.size() < min_heap.size())
		{
			if (val > med)
			{
				max_heap.push(val);
			}
			else
			{
				max_heap.push(min_heap.top());
				min_heap.pop();
				min_heap.push(val);
			}
			med = (max_heap.top() + min_heap.top()) / 2;
		}
		else if (max_heap.size() == min_heap.size())
		{
			if (val<med)
			{
				min_heap.push(val);
				med = min_heap.top();
			}
			else
			{
				max_heap.push(val);
				med = max_heap.top();
			}

		}
	}

}

int main()
{
	// stream of integers 
	double arr[] = { 5, 15, 10, 20, 3 };
	int n = sizeof(arr) / sizeof(arr[0]);
	CalculateMedians(arr, n);
	return 0;
}
