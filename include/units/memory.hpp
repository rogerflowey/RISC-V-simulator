#pragma once
#include "constants.hpp"
#include "utils/buffered.hpp"
#include "utils/clock.hpp"
#include "utils/logger/logger.hpp"
#include "utils/uints.hpp"

enum MemoryRequestType{
    READ,WRITE
};

struct MemoryRequest{
    MemoryRequestType type;
    RobIDType rob_id;
    RegType address;
    u_int8_t size;
    MemDataType data;
};

struct MemoryResponse{
    RobIDType rob_id;
    MemDataType data;
};

class Memory{
    alignas(8) std::byte memory[MEMORY_SIZE]{};
    int time_cnt;
    Buffered<MemoryRequest> request;
    Buffered<MemoryResponse> response;
public:
    Memory(){
        Clock::getInstance().subscribe([this]{this->tick();});
    }

    void req(MemoryRequest request) {
        if (time_cnt == 0) {
            this->request <= request;
            time_cnt = 3;
        } else {
            throw logger().Error("Memory request in progress");
        }
    }

    bool idle(){
        return time_cnt==0;
    }

    MemoryResponse get_response(){
        return *response;
    }
    void consume(){
        response <= MemoryResponse{};
    }

    void tick(){
        if(time_cnt>0){
            time_cnt--;
            if(time_cnt==0){
                if(request->type==READ){
                    response <= MemoryResponse{request->rob_id, bytes_to_uint(&memory[request->address], &memory[request->address + request->size])};
                } else{
                    response <= MemoryResponse{request->rob_id, 0};
                }
            }
        }
    }
};