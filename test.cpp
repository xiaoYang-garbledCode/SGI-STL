#include<vector>
#include<iostream>
#include"myallocator.h"

int main() {
	std::vector<int, myallocator<int>> vec;
	int data;
	for (int i = 0; i < 100; ++i)
	{
		if (i == 10)
		{
			std::cout<<" ";
		}
		data = rand() % 100;
		vec.push_back(data);
	} 
	for (int x : vec) {
		std::cout << x << " ";
	}
}
