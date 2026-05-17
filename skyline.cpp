//
// skyline building
//

#include <vector>
#include <algorithm>
#include <queue>
#include <iostream>

struct building
{
	int x;
	int y;
	int w;
};

struct skyline_input
{
	enum building_state
	{
		START = 0,
		END = 1
	};
	int x;
	int y;
	building_state state;
};

struct skyline_output
{
	int x;
	int y;
};

struct skyline_input_operator
{
	int operator() (const skyline_input& s1, const skyline_input& s2)
	{
		return s1.x < s2.x;
	}
};

static void make_skyline_input(const std::vector<building>& array_building, std::vector<skyline_input>& results)
{
	for (auto x : array_building)
	{
		skyline_input x1, x2;
		x1.x = x.x;
		x1.y = x.y;
		x1.state = skyline_input::START;

		x2.x = x.x + x.w;
		x2.y = x.y;
		x2.state = skyline_input::END;

		results.push_back(x1);
		results.push_back(x2);
	}
}

void skyline(std::vector<building>& array_building)
{
	std::vector<skyline_input> results;
	make_skyline_input(array_building, results);
	std::sort(results.begin(), results.end(), skyline_input_operator());

	std::priority_queue<int> heap;
	heap.push(0);
	int current_max = 0;
	std::vector<skyline_output> skyline_output_result;
	for (auto x : results)
	{
		skyline_output h;
		if (x.state == skyline_input::START)
		{
			heap.push(x.y);
			int top = heap.top();
			if (top != current_max)
			{
				h.x = x.x;
				current_max = top;
				h.y = current_max;
				skyline_output_result.push_back(h);
			}
		}
		else if (x.state == skyline_input::END)
		{
			heap.pop();
			int top = heap.top();
			if (top != current_max)
			{
				h.x = x.x;
				current_max = top;
				h.y = current_max;
				skyline_output_result.push_back(h);
			}
		}
	}

	for (auto y : skyline_output_result)
	{
		std::cout << y.x << "," << y.y << std::endl;
	}
}

int main()
{
	//std::vector<building> b = { {5,9,3}, {2,5,4}, {8, 10,9} };

	std::vector<building> b = { { 1,4,2 },{ 2,3,2 } };
	skyline(b);
}