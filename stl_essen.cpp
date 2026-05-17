#include<iostream>
#include <vector> 
#include<algorithm> // sort, find, nth_element
#include<numeric> // accumulate, iota
#include <cstdint>

#define MAX_DATA 1000

int main(int argc, char** argv)
{
    std::vector<uint16_t> data_reserve;
    data_reserve.reserve(MAX_DATA);
    for (uint16_t i = 0; i < MAX_DATA; ++i)
    {
        data_reserve.push_back(i);
    }
    for (auto x: data_reserve)
    {
        std::cout << "x=" << x <<std::endl;
    }

    std::vector<uint16_t> data_resize;
    data_resize.resize(MAX_DATA);
    for (int i = 0; i < MAX_DATA; ++i)
    {
        data_resize[i] = i+MAX_DATA;
    }
   
    for (auto x: data_resize)
    {
        std::cout << "x=" << x <<std::endl;
    }

    return 0;
}