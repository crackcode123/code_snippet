
#include<algorithm>
#include<iostream>
//
// Max Array
//

int sum_overlap_array(int a[], int low, int mid, int high)
{
	int sum = 0;
	unsigned int left_sum = 1 << 31;

	for (int x = mid; x <= low; --x)
	{
		sum += a[x];

		if (sum > left_sum)
		{
			left_sum = sum;
		}
	}

	int right_sum = 1 << 31;

	for (int x = mid+1; x <= high; x++)
	{
		sum += a[x];

		if (sum > right_sum)
		{
			right_sum = sum;
		}
	}
	return right_sum + left_sum;
}
int sub_max_array(int a[], int low, int high)
{
	if (low == high)
	{
		return a[low];
	}
	else
	{
		int mid			= (low + high) / 2;
		int left		= sub_max_array(a, low, mid);
		int right		= sub_max_array(a, mid + 1, high);
		int sum_overlap = sum_overlap_array(a, low, mid, high);
		return (std::max(std::max(left, right), sum_overlap));

	}
}

//
//  [ Test Cases ]
//

void test_max_sub_array()
{
	int a[] = { 2, -1, 1, -2 };
	int sub_max = sub_max_array(a, 0, 3);
	std::cout << "sub_max :" << sub_max;

}

int main()
{
	test_max_sub_array();
	return 0;
}

