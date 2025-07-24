#pragma once

#include "cdb.hpp"
#include "constants.hpp"
#include "utils/clock.hpp"
#include <utility>





class RegisterFile{
    RegType reg[REG_SIZE];
    RobIDType rename[REG_SIZE];
    CommonDataBus& cdb;

public:
    RegisterFile(CommonDataBus& cdb) : cdb(cdb) {
        Clock::getInstance().subscribe([this]{ this->tick(); });
    }

    void set(RegIDType id, RegType value){
        reg[id] = value;
    }
    std::pair<RegType,RobIDType> get(RegIDType id){
        return {reg[id],rename[id]};
    }
    void preset(RegIDType id, RobIDType rob_id){
        rename[id] = rob_id;
    }

    void tick(){
        auto result = cdb.get();
        for(int i = 0; i < REG_SIZE; i++){
            if(rename[i] == result.first){
                reg[i] = result.second;
            }
        }
    }
};