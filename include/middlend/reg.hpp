#pragma once

#include "backend/cdb.hpp"
#include "constants.hpp"
#include "utils/buffered.hpp"
#include "utils/clock.hpp"
#include <utility>




class RegisterFile{
    std::array<Buffered<RegDataType>, REG_SIZE> reg;
    std::array<Buffered<RobIDType>, REG_SIZE> rename;
    CommonDataBus& cdb;

public:
    RegisterFile(CommonDataBus& cdb) : cdb(cdb) {
        Clock::getInstance().subscribe([this]{ this->tick(); });
    }

    std::pair<RegDataType,RobIDType> get(RegIDType id){
        return {*reg[id],*rename[id]};
    }
    void preset(RegIDType id, RobIDType rob_id){
        rename[id] <= rob_id;
    }

    void tick(){
        auto result = cdb.get();
        if(result){
            for(int i = 0; i < REG_SIZE; i++){
                if(*rename[i] == result->rob_id){
                    reg[i] <= result->data;
                }
            }
        }
    }
};