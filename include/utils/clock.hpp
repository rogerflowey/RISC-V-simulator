#pragma once
#include <cstddef>
#include <vector>
#include <functional>

enum Edge{
    RISING,
    FALLING
};


class Clock{
    std::vector<std::function<void()>> subscribers;
    std::vector<std::function<void()>> tock_sub;
    size_t current_time;

public:
    Clock() : current_time(0) {}

    static Clock& getInstance() {
        static Clock instance;
        return instance;
    }

    void reset() {
        current_time = 0;
        subscribers.clear();
        tock_sub.clear();
    }

    void tick() {
        current_time++;

        //should shuffle
        for (auto& subscriber : subscribers) {
            subscriber();
        }
        for (auto& subscriber : tock_sub) {
            subscriber();
        }
    }

    void subscribe(std::function<void()> callback, Edge edge = RISING) {
        if (edge == RISING) {
            subscribers.push_back(callback);
        } else {
            tock_sub.push_back(callback);
        }
    }

    size_t getTime(){
        return current_time;
    }
};