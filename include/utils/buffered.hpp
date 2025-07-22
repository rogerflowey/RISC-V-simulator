#pragma once
#include "clock.hpp"


template <typename T>
class Buffered {
    T value;
    T new_value;

public:
    Buffered(const T& v):value(v),new_value(v){
        Clock::getInstance().subscribe([this]() { this->commit(); });
    }

    void update(T new_val) {
        new_value = new_val;
    }

    void commit() {
        value = new_value;
    }

    T get() const {
        return value;
    }

    operator T() const {
        return get();
    }

    void operator<=(const T& new_val) {
        update(new_val);
    }
};
