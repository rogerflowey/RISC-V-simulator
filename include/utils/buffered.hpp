#pragma once

#include <type_traits>

#include "clock.hpp"



template <typename T>
requires std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>
class Buffered {
    T value;
    T new_value;

public:
    Buffered():value(),new_value(){
        Clock::getInstance().subscribe([this]() { this->commit(); },FALLING);
    }

    Buffered(const T& v):value(v),new_value(v){
        Clock::getInstance().subscribe([this]() { this->commit(); },FALLING);
    }

    template<typename... Args>
    Buffered(Args&&... args):value(std::forward<Args>(args)...),new_value(std::forward<Args>(args)...){
        Clock::getInstance().subscribe([this]() { this->commit(); },FALLING);
    }

    
    void commit() {
        value = new_value;
    }

    void operator<=(const T& new_val) {
        new_value = new_val;
    }


    //read only
    const T* operator->() const {
        return &value;
    }
    const T& operator*() const {
        return value;
    }


    //write is only available for new_value
    T& next() {
        return new_value;
    }
};
