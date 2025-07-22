#pragma once
#include <vector>
#include <functional>


class Clock{
    std::vector<std::function<void()>> subscribers;
    size_t current_time;

public:
    Clock() : current_time(0) {}

    static Clock& getInstance() {
        static Clock instance;
        return instance;
    }

    void tick() {
        current_time++;

        //should shuffle
        for (auto& subscriber : subscribers) {
            subscriber();
        }
    }

    void subscribe(std::function<void()> callback) {
        subscribers.push_back(callback);
    }
};