#pragma once

#include <cstddef>
#include <stdexcept>
#include <type_traits>

template<typename T, size_t MAX_SIZE>
class queue {
    static_assert(MAX_SIZE > 0, "Queue size must be greater than 0");
    
private:
    T data[MAX_SIZE]{};
    size_t _front = 0;
    size_t _back = 0;
    size_t _size = 0;

    static constexpr size_t next_index(size_t index) noexcept {
        return (index + 1) % MAX_SIZE;
    }
    
    static constexpr size_t prev_index(size_t index) noexcept {
        return (index == 0) ? MAX_SIZE - 1 : index - 1;
    }

public:
    using value_type = T;
    using size_type = size_t;
    using reference = T&;
    using const_reference = const T&;
    
    queue() = default;
    
    queue(const queue& other) : _front(other._front), _back(other._back), _size(other._size) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::copy(other.data, other.data + MAX_SIZE, data);
        } else {
            for (size_t i = 0; i < MAX_SIZE; ++i) {
                data[i] = other.data[i];
            }
        }
    }
    
    queue& operator=(const queue& other) {
        if (this != &other) {
            _front = other._front;
            _back = other._back;
            _size = other._size;
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::copy(other.data, other.data + MAX_SIZE, data);
            } else {
                for (size_t i = 0; i < MAX_SIZE; ++i) {
                    data[i] = other.data[i];
                }
            }
        }
        return *this;
    }
    
    ~queue() = default;

    [[nodiscard]] reference front() {
        if (empty()) {
            throw std::runtime_error("Queue is empty");
        }
        return data[_front];
    }
    
    [[nodiscard]] const_reference front() const {
        if (empty()) {
            throw std::runtime_error("Queue is empty");
        }
        return data[_front];
    }
    
    [[nodiscard]] reference back() {
        if (empty()) {
            throw std::runtime_error("Queue is empty");
        }
        return data[prev_index(_back)];
    }
    
    [[nodiscard]] const_reference back() const {
        if (empty()) {
            throw std::runtime_error("Queue is empty");
        }
        return data[prev_index(_back)];
    }

    // Capacity
    [[nodiscard]] bool empty() const noexcept {
        return _size == 0;
    }
    
    [[nodiscard]] bool full() const noexcept {
        return _size == MAX_SIZE;
    }
    
    [[nodiscard]] size_type size() const noexcept {
        return _size;
    }
    
    [[nodiscard]] static constexpr size_type max_size() noexcept {
        return MAX_SIZE;
    }
    
    [[nodiscard]] size_type capacity() const noexcept {
        return MAX_SIZE;
    }

    void push_back(const T& value) {
        if (full()) {
            throw std::runtime_error("Queue is full");
        }
        data[_back] = value;
        _back = next_index(_back);
        ++_size;
    }
    
    void push_back(T&& value) {
        if (full()) {
            throw std::runtime_error("Queue is full");
        }
        data[_back] = std::move(value);
        _back = next_index(_back);
        ++_size;
    }
    
    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (full()) {
            throw std::runtime_error("Queue is full");
        }
        data[_back] = T(std::forward<Args>(args)...);
        _back = next_index(_back);
        ++_size;
    }
    
    void pop_back() {
        if (empty()) {
            throw std::runtime_error("Queue is empty");
        }
        _back = prev_index(_back);
        --_size;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            data[_back].~T();
        }
    }

    void push_front(const T& value) {
        if (full()) {
            throw std::runtime_error("Queue is full");
        }
        _front = prev_index(_front);
        data[_front] = value;
        ++_size;
    }
    
    void push_front(T&& value) {
        if (full()) {
            throw std::runtime_error("Queue is full");
        }
        _front = prev_index(_front);
        data[_front] = std::move(value);
        ++_size;
    }
    
    template<typename... Args>
    void emplace_front(Args&&... args) {
        if (full()) {
            throw std::runtime_error("Queue is full");
        }
        _front = prev_index(_front);
        data[_front] = T(std::forward<Args>(args)...);
        ++_size;
    }
    
    void pop_front() {
        if (empty()) {
            throw std::runtime_error("Queue is empty");
        }
        if constexpr (!std::is_trivially_destructible_v<T>) {
            data[_front].~T();
        }
        _front = next_index(_front);
        --_size;
    }
    
    void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            while (!empty()) {
                pop_front();
            }
        } else {
            _front = 0;
            _back = 0;
            _size = 0;
        }
    }
    
    void swap(queue& other) noexcept {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::swap(data, other.data);
        } else {
            for (size_t i = 0; i < MAX_SIZE; ++i) {
                std::swap(data[i], other.data[i]);
            }
        }
        std::swap(_front, other._front);
        std::swap(_back, other._back);
        std::swap(_size, other._size);
    }
    
    bool operator==(const queue& other) const {
        if (_size != other._size) return false;
        
        size_t my_idx = _front;
        size_t other_idx = other._front;
        
        for (size_t i = 0; i < _size; ++i) {
            if (data[my_idx] != other.data[other_idx]) {
                return false;
            }
            my_idx = next_index(my_idx);
            other_idx = next_index(other_idx);
        }
        return true;
    }
    
    bool operator!=(const queue& other) const {
        return !(*this == other);
    }
};
