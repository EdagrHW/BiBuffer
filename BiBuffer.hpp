#pragma once

#include<mutex>
#include<atomic>
#include<vector>
#include<algorithm>
#include<chrono>

#include<iostream>

template<typename T>
class BiBuffer
{
public:
	explicit BiBuffer(unsigned int size);
	std::shared_ptr<T> read();
	void write(std::shared_ptr<T> data);
	~BiBuffer() = default;

private:
	unsigned int _size; //单个缓冲区大小
	std::atomic<unsigned int> _readIdx; //读取下标
	std::atomic<unsigned int> _writeIdx; //写入下标
	std::vector<std::shared_ptr<T>> _buffer_0; //0号缓冲区
	std::vector<std::shared_ptr<T>> _buffer_1; //1号缓冲区
	std::atomic<int> _rwFlag_0; // 0：0号缓冲区可写，1：0号缓冲区可读
	std::atomic<int> _rwFlag_1; // 0：1号缓冲区可写，1：1号缓冲区可读
};

template<typename T>
inline BiBuffer<T>::BiBuffer(unsigned int size)
	: _size(size), _readIdx(0), _writeIdx(0)
	, _buffer_0(size), _buffer_1(size)
	, _rwFlag_0(0), _rwFlag_1(0)
{
}
template<typename T>
inline std::shared_ptr<T> BiBuffer<T>::read()
{
	//没有可以读取数据的缓冲区
	while (_rwFlag_0.load() != 1 && _rwFlag_1.load() != 1)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	//之前读取的是0号缓冲区
	if (_readIdx.load() < _size && _rwFlag_0.load() == 1)
	{
		std::shared_ptr<T> res = _buffer_0[_readIdx];
		//0号缓冲区读完了，修改读取位置和读写标识
		if (++_readIdx >= _size)
		{
			_rwFlag_0.store(0);
			std::cout << "0号缓冲区读完了，修改读取位置和读写标识" << std::endl;
		}
		return res;
	}
	//之前读取的是1号缓冲区
	else if (_readIdx.load() >= _size && _rwFlag_0.load() == 1)
	{
		std::shared_ptr<T> res = _buffer_1[_readIdx - _size];
		//1号缓冲区读完了，修改读取位置和读写标识
		if (++_readIdx >= 2 * _size)
		{
			_rwFlag_1.store(0);
			_readIdx.store(0);
			std::cout << "1号缓冲区读完了，修改读取位置和读写标识" << std::endl;
		}
		return res;
	}
	else
	{
		std::cout << "标志位错乱, _rwFlag_0: " << _rwFlag_0
			<< "_rwFlag_1" <<_rwFlag_1 
			<<"readIdx:" << _readIdx.load()
			<< std::endl;
		return std::shared_ptr<T>();
	}
}
template<typename T>
inline void BiBuffer<T>::write(std::shared_ptr<T> data)
{
	//没有可以写的缓冲区
	while (_rwFlag_0.load() != 0 && _rwFlag_1.load() != 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	if (_writeIdx.load() < _size && _rwFlag_0.load() == 0)
	{
		_buffer_0[_writeIdx] = std::move(data);
		if (++_writeIdx >= _size)
		{
			std::cout << "0号缓冲区写完了，修改写入位置和读写标识" << std::endl;
			_rwFlag_0.store(1);
		}
	}
	else if (_writeIdx.load() >= _size && _rwFlag_1.load() == 0)
	{
		_buffer_1[_writeIdx - _size] = std::move(data);
		if (++_writeIdx >= 2 * _size)
		{
			std::cout << "1号缓冲区写完了，修改写入位置和读写标识" << std::endl;
			_rwFlag_0.store(1);
			_writeIdx.store(0);
		}
	}
}
;
