#include "BiBuffer.hpp"
#include "MutiBuffer.hpp"
#include<thread>

using namespace std;
void loadData(Buffer<size_t>& nums, size_t begin, size_t end)
{
	for (size_t i = begin; i < end; i++)
	{
		nums.push_back(std::make_shared<size_t>(i));
	}
	std::this_thread::sleep_for(std::chrono::seconds(20));
}

int main()
{

	MutiBuffer<size_t> mi(4, 10);

	thread t1([&] 
		{
			size_t begin = 0;
			size_t end = 10;
			while (true)
			{
				LoadDataFunc<size_t> func = std::bind(loadData, std::placeholders::_1, begin, end);
				mi.write(func);
				begin += 10;
				end += 10;
			}
		});

	thread t2([&]
		{
			while (true)
			{

				time_t start, end;
				start = time(NULL);
				Buffer<size_t> datas;
				mi.waitAndRead(datas);
				//模拟数据处理的时长
				std::this_thread::sleep_for(std::chrono::seconds(5));
				end = time(NULL);
				cout << "读取一个缓冲区花费：" << end - start << endl;
				for (size_t i = 0; i < datas.size(); i++)
				{
					cout << *(datas[i]) << endl;
				}
			}
		});

	t1.join();
	t2.join();

	//BiBuffer<int> a(1024);

	//thread t1([&]
	//	{
	//		int i = 0;
	//		while (true)
	//		{
	//			std::this_thread::sleep_for(std::chrono::microseconds(10));
	//			a.write(std::make_shared<int>(i++));
	//		}
	//		
	//	});

	//thread t2([&]
	//	{
	//		int i = 0;
	//		while (true)
	//		{
	//			
	//			a.read();
	//			//std::cout << a.read() << std::endl;
	//		}

	//	});
	//t1.join();
	//t2.join();
	return 1;
}