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
	void writeBuffer(LoadDataFunc<T> loadDataFunc, size_t taskId);
	~MutiBuffer() = default;
private:
	unsigned int _writeThreadNum; //д�߳���
	unsigned int _bufferSize; //ÿ��buffer�Ĵ�С
	std::vector<Buffer<T>> _buffers; //buffers
	ThreadPool _threadPool; //�̳߳�
	std::vector<int> _bufferEmploy; //������ռ��״̬
	std::mutex _bufferStatusMutex; //������״̬�ٽ���
	int _readBufferIndex; //��ǰ��ȡ�Ļ��������±�
	int _writeBufferIndex; //��ǰд�Ļ��������±�
};

template<typename T>
inline MutiBuffer<T>::MutiBuffer(unsigned int writeThreadNum, unsigned int bufferSize)
	:_writeThreadNum(writeThreadNum), _bufferSize(bufferSize)
	, _threadPool(writeThreadNum), _readBufferIndex(0), _writeBufferIndex(0)
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
				std::cout << "����" << taskId << ",��ʼд����" << std::endl;
#endif // DEBUG
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
	
}


