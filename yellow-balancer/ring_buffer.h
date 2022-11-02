#pragma once

#include <vector>
#include <numeric>

template <class T>
class RingBuffer {
public:
	RingBuffer(size_t size);
	void Add(T value);
	size_t Size() { return size_; }
	size_t Capacity() { return buffer_.size(); }
	T Avg();
private:
	std::vector<T> buffer_;
	size_t index_;
	size_t size_;
};

template <typename T>
RingBuffer<T>::RingBuffer(size_t size) {
	buffer_.resize(size);
	index_ = 0;
	size_ = 0;
}

template <typename T>
void RingBuffer<T>::Add(T value) {
	buffer_[index_] = value;
	++index_;
	if (index_ >= buffer_.size()) index_ = 0;
	if (size_ < buffer_.size()) ++size_;
}

template <typename T>
T RingBuffer<T>::Avg() {
	T zero = 0;
	return std::accumulate(buffer_.begin(), buffer_.begin() + size_, zero) / size_;
}