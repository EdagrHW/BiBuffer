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
class SingleBuffer
{
public:
	explicit SingleBuffer(size_t size);
	std::shared_ptr<T> read();
	void write(std::shared_ptr<T> value);
	bool writable();
	bool readable();
	~SingleBuffer() = default;

private:
	size_t _size;
	std::vector<std::shared_ptr<T>> _buffer;
	std::atomic_int _rwFlag; //读写标标识
	std::atomic_uint _index; //读写位置
};

template<typename T>
inline SingleBuffer<T>::SingleBuffer(size_t size)
	: _size(size), _buffer(size), _rwFlag(1)
{
}

template<typename T>
inline std::shared_ptr<T> SingleBuffer<T>::read()
{
	if (_rwFlag.load() == BUFFER_CAN_READ)
	{
		std::shared_ptr<T> res = std::move(_buffer[_index]);
		if (++_index >= _size)
		{
			_index.store(0);
			_rwFlag.store(BUFFER_CAN_WRITE);
		}
		return res;
	}

	return std::shared_ptr<T>();
	
}

template<typename T>
inline void SingleBuffer<T>::write(std::shared_ptr<T> value)
{
	if (_rwFlag.load() == BUFFER_CAN_WRITE)
	{ 
		_buffer[_index] = std::move(value);
		if (++_index >= _size)
		{
			_index.store(0);
			_rwFlag.store(BUFFER_CAN_READ);
		}
	}
}

template<typename T>
inline bool SingleBuffer<T>::writable()
{
	return _rwFlag.load() == BUFFER_CAN_WRITE;
}

template<typename T>
inline bool SingleBuffer<T>::readable()
{
	return _rwFlag.load() == BUFFER_CAN_READ;
}

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
	void writeBuffer(LoadDataFunc<T> loadDataFunc);
	size_t getWriteBufferIndex();
	~MutiBuffer() = default;
private:
	unsigned int _writeThreadNum; //写线程数
	unsigned int _bufferSize; //每个buffer的大小
	std::vector<Buffer<T>> _buffers; //buffers
	ThreadPool _threadPool; //线程池
	std::vector<int> _bufferEmploy; //缓冲区占用状态
	std::mutex _bufferStatusMutex; //缓冲区状态临界区
	int _readBufferIndex; //当前读取的缓冲区的下标
	std::atomic_int _writeBufferIndex; //当前写的缓冲区的下标
};

template<typename T>
inline MutiBuffer<T>::MutiBuffer(unsigned int writeThreadNum, unsigned int bufferSize)
	:_writeThreadNum(writeThreadNum), _bufferSize(bufferSize)
	, _threadPool(writeThreadNum), _readBufferIndex(0)
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
	while (_threadPool.getTaskNums() >= _buffers.size())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	
	_threadPool.enqueue(&MutiBuffer::writeBuffer, this, loadDataFunc);
	/*size_t num = datas.size() / _bufferSize;
	for (size_t i = 0; i < num; i++)
	{
		_threadPool.enqueue(&MutiBuffer::writeBuffer, this
			, std::ref(datas), i * _bufferSize, (i + 1) * _bufferSize);
	}*/

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
	//static int index = 0;
	//static time_t start = time(NULL);
	//static time_t end = time(NULL);
	//while (true)
	//{
	//	std::shared_ptr<T> res = _buffers[_readBufferIndex]->read();
	//	if (res)
	//	{
	//		if (index == 0)
	//		{
	//			end = time(NULL);
	//			std::cout << "缓存区：" << _readBufferIndex << "等待了数据时长：" << end - start << std::endl;
	//			start = time(NULL);
	//		}

	//		index++;
	//		return res;
	//	}
	//	else if (index >= _bufferSize - 1)
	//	{
	//		std::cout << "缓存区：" << _readBufferIndex << "读完" << std::endl;
	//		end = time(NULL);
	//		std::cout << "读取一个缓冲区耗费时长: " << end - start << std::endl;
	//		start = time(NULL);
	//		index=0;
	//		//释放当前buffer的占用
	//		releaseWriteBufferIndex(_readBufferIndex);
	//		_readBufferIndex = (++_readBufferIndex) % _buffers.size();
	//	}
	//	/*else
	//	{
	//		std::this_thread::sleep_for(std::chrono::microseconds(1));
	//	}*/
	//	else
	//	{
	//		return res;
	//	}
	//	
	//}
}

template<typename T>
inline void MutiBuffer<T>::writeBuffer(LoadDataFunc<T> loadDataFunc)
{
	size_t curBufferIndex = getWriteBufferIndex();
	while (curBufferIndex == -1)
	{
		std::this_thread::sleep_for(std::chrono::microseconds(10));
		curBufferIndex = getWriteBufferIndex();
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
	std::cout << "缓冲区:" << curBufferIndex << "，写完" << std::endl;
	//for (size_t i = 0; i < datas.size(); i++)
	//{
	//	_buffers[curBufferIndex]->write(std::move(datas[i]));
	//}
	
}

template<typename T>
inline size_t MutiBuffer<T>::getWriteBufferIndex()
{
	std::lock_guard<std::mutex> lk(_bufferStatusMutex);
	size_t writeBuffIndex = 0;
	size_t lastWriteBuffIndex = 0;
	for (size_t i = 0; i < _bufferEmploy.size(); i++)
	{
		if (_bufferEmploy[i] != BUFFER_IDEL)
		{
			lastWriteBuffIndex = i;
		}
	}
	if (lastWriteBuffIndex != 0 || _bufferEmploy[0] != BUFFER_IDEL)
	{
		writeBuffIndex = (lastWriteBuffIndex + 1) % _bufferEmploy.size();
	}
	if (_bufferEmploy[writeBuffIndex] == BUFFER_IDEL)
	{
		_bufferEmploy[writeBuffIndex] = BUFFER_CAN_WRITE;
		return writeBuffIndex;
	}
	else
	{
		return -1;
	}

	/*for (size_t i = 0; i < _bufferEmploy.size()-1; i++)
	{
		if (_bufferEmploy[i] == BUFFER_CAN_WRITE && _bufferEmploy[i+1] != BUFFER_CAN_WRITE)
		{
			writeBuffIndex = i + 1;
			_bufferEmploy[writeBuffIndex] = BUFFER_CAN_WRITE;
			return writeBuffIndex;
		}
	}
	if (_bufferEmploy[writeBuffIndex] == BUFFER_IDEL)
	{
		_bufferEmploy[writeBuffIndex] = BUFFER_CAN_WRITE;
		return writeBuffIndex;
	}
	return -1;*/
}


