#pragma once

#include "utils/clock.hpp"
#include <optional>



// this is a one consume reader(maybe multiple unconsumed), one-writer channel
template<typename T>
class Channel{
    T reader_slot;
    T writer_slot;
    bool reader_ready = false;
    bool writer_ready = false;
    bool consumed = false;
public:
    Channel(){
        Clock::getInstance().subscribe([this]() { this->tick();},FALLING);
    }
    bool send(const T& data){
        if(writer_ready){
            return false;
        }
        writer_slot = data;
        writer_ready = true;
        return true;
    }
    std::optional<const T&> peek(){
        if(!reader_ready) return std::nullopt;
        return reader_slot;
    }
    std::optional<const T&> receive(){
        if(!reader_ready) return std::nullopt;
        consumed = true;
        return reader_slot;
    }
    void tick(){
        if(consumed){
            reader_ready = false;
            consumed = false;
        }
        if(!reader_ready && writer_ready){
            reader_ready = true;
            writer_ready = false;
            reader_slot = writer_slot;
        }
    }
};



template<typename T>
class Bus{
    Channel<T> channel;
public:
    Bus(){
        Clock::getInstance().subscribe([this]() {channel.receive();},RISING);
    }
    bool send(const T& data){
        return channel.send(data);
    }
    std::optional<const T&> get(){
        return channel.peek();
    }
};