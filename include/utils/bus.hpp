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
    bool can_send() const {
        return !writer_ready;
    }
    bool send(const T& data){
        if(writer_ready){
            return false;
        }
        writer_slot = data;
        writer_ready = true;
        return true;
    }
    std::optional<T> peek(){
        if(!reader_ready) return std::nullopt;
        return reader_slot;
    }
    std::optional<T> receive(){
        if(!reader_ready) return std::nullopt;
        consumed = true;
        return reader_slot;
    }
    void reader_clear(){
        reader_ready = false;
        consumed = false;
    }
    void writer_clear(){
        writer_ready = false;
    }

    //legacy, should be removed
    void clear() {
        reader_ready = false;
        writer_ready = false;
        consumed = false;
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
class HandshakeChannel {
private:
    T slot;
    bool reader_is_ready = false;
    bool data_is_valid = false;

public:
    HandshakeChannel() = default;

    bool can_send() const {
        return reader_is_ready && !data_is_valid;
    }

    bool send(const T& data) {
        if (!can_send()) {
            return false;
        }
        slot = data;
        data_is_valid = true;
        reader_is_ready = false;
        return true;
    }

    void ready() {
        reader_is_ready = true;
    }
    std::optional<T> receive() {
        if (data_is_valid) {
            data_is_valid = false;
            return slot;
        }
        return std::nullopt;
    }
    void clear() {
        reader_is_ready = false;
        data_is_valid = false;
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
    std::optional<T> get(){
        return channel.peek();
    }
};