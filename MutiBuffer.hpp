#pragma once
#include<atomic>
#include<chrono>
#include<vector>
#include<memory>
#include<cassert>
#include<thread>
#include<functional>

#include "ThreadPool.hpp"

/*
* 多个buffer缓冲区，多个线程写，一个线程读
，目的是解决读快写慢的问题,尽量缩小读写速度差
*/
const int BUFFER_CAN_READ = 0; //缓冲区可以读
const int BUFFER_CAN_WRITE = 1; //缓冲区可以写
const int BUFFER_IDEL = -1; //缓冲区空闲

template<typename T>
using Buffer = std::vector<std::shared_ptr<T>>;

template<typename T>
using LoadDataFunc = std::function<void(Buffer<T>& datas)>;

template<typename T>
class MutiBuffer
{
public:
	explicit MutiBuffer(unsigned int writeThreadNum = 2, unsigned int bufferSize = 1024);
	void write(LoadDataFunc<T> loadDataFunc);
	void waitAndRead(Buffer<T>& datas);
	void writeBuffer(LoadDataFunc<T> loadDataFunc, size_t taskId);
	~MutiBuffer() = default;
private:
	unsigned int _writeThreadNum; //写线程数
	unsigned int _bufferSize; //每个buffer的大小
	std::vector<Buffer<T>> _buffers; //buffers
	ThreadPool _threadPool; //线程池
	std::vector<int> _bufferEmploy; //缓冲区占用状态
	std::mutex _bufferStatusMutex; //缓冲区状态临界区
	int _readBufferIndex; //当前读取的缓冲区的下标
	int _writeBufferIndex; //当前写的缓冲区的下标
};

template<typename T>
inline MutiBuffer<T>::MutiBuffer(unsigned int writeThreadNum, unsigned int bufferSize)
	:_writeThreadNum(writeThreadNum), _bufferSize(bufferSize)
	, _threadPool(writeThreadNum), _readBufferIndex(0), _writeBufferIndex(0)
{
	assert(writeThreadNum >= 1);
	//缓冲区个数是写线程数的两倍
	for (size_t i = 0; i < writeThreadNum * 2; i++)
	{
		_buffers.push_back(Buffer<T>(bufferSize));
		_bufferEmploy.push_back(BUFFER_IDEL);
	}

}

template<typename T>
inline void MutiBuffer<T>::write(LoadDataFunc<T> loadDataFunc)
{
	//任务数量超过缓冲区个数就等待
	static size_t taskNums = 0;
	while (_threadPool.getTaskNums() >= _buffers.size())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	
	_threadPool.enqueue(&MutiBuffer::writeBuffer, this, loadDataFunc, taskNums);
	taskNums = (taskNums + 1) % _buffers.size();

}

template<typename T>
void MutiBuffer<T>::waitAndRead(Buffer<T>& datas)
{
	while (true)
	{
		{
			std::lock_guard<std::mutex> lk(_bufferStatusMutex);
			if (_bufferEmploy[_readBufferIndex] == BUFFER_CAN_READ)
			{
				datas.swap(_buffers[_readBufferIndex]);
				_bufferEmploy[_readBufferIndex] = BUFFER_IDEL;
				_readBufferIndex = (++_readBufferIndex) % _buffers.size();
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::microseconds(10));

	}
}

template<typename T>
inline void MutiBuffer<T>::writeBuffer(LoadDataFunc<T> loadDataFunc, size_t taskId)
{

	size_t curBufferIndex = 0;
	while (true)
	{
		{
			std::lock_guard<std::mutex> lk(_bufferStatusMutex);
			if (_bufferEmploy[taskId] == BUFFER_IDEL)
			{
				curBufferIndex = taskId;
#ifdef DEBUG
				std::cout << "任务" << taskId << ",开始写数据" << std::endl;
#endif // DEBUG
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	

	//调用函数区读取数据
	Buffer<T> datas;
	loadDataFunc(datas);

	if (datas.empty())
	{
		return;
	}
	assert(datas.size() <= _bufferSize);
	_buffers[curBufferIndex].swap(datas);
	std::lock_guard<std::mutex> lk(_bufferStatusMutex);
	_bufferEmploy[curBufferIndex] = BUFFER_CAN_READ;
	
}


