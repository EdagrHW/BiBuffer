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
* ���buffer������������߳�д��һ���̶߳�
��Ŀ���ǽ������д��������,������С��д�ٶȲ�
*/
const int BUFFER_CAN_READ = 0; //���������Զ�
const int BUFFER_CAN_WRITE = 1; //����������д
const int BUFFER_IDEL = -1; //����������

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
	unsigned int _writeThreadNum; //д�߳���
	unsigned int _bufferSize; //ÿ��buffer�Ĵ�С
	std::vector<Buffer<T>> _buffers; //buffers
	ThreadPool _threadPool; //�̳߳�
	std::vector<int> _bufferEmploy; //������ռ��״̬
	std::mutex _bufferStatusMutex; //������״̬�ٽ���
	int _readBufferIndex; //��ǰ��ȡ�Ļ��������±�
	std::atomic_int _writeBufferIndex; //��ǰд�Ļ��������±�
};

template<typename T>
inline MutiBuffer<T>::MutiBuffer(unsigned int writeThreadNum, unsigned int bufferSize)
	:_writeThreadNum(writeThreadNum), _bufferSize(bufferSize)
	, _threadPool(writeThreadNum), _readBufferIndex(0)
{
	assert(writeThreadNum >= 1);
	//������������д�߳���������
	for (size_t i = 0; i < writeThreadNum * 2; i++)
	{
		_buffers.push_back(Buffer<T>(bufferSize));
		_bufferEmploy.push_back(BUFFER_IDEL);
	}

}

template<typename T>
inline void MutiBuffer<T>::write(LoadDataFunc<T> loadDataFunc)
{
	//�����������������������͵ȴ�
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
	//			std::cout << "��������" << _readBufferIndex << "�ȴ�������ʱ����" << end - start << std::endl;
	//			start = time(NULL);
	//		}

	//		index++;
	//		return res;
	//	}
	//	else if (index >= _bufferSize - 1)
	//	{
	//		std::cout << "��������" << _readBufferIndex << "����" << std::endl;
	//		end = time(NULL);
	//		std::cout << "��ȡһ���������ķ�ʱ��: " << end - start << std::endl;
	//		start = time(NULL);
	//		index=0;
	//		//�ͷŵ�ǰbuffer��ռ��
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

	//���ú�������ȡ����
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
	std::cout << "������:" << curBufferIndex << "��д��" << std::endl;
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


