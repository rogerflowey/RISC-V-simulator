#pragma once

#include "constants.hpp"

#include <utility>
#include <array>




class RegisterFile{
    std::array<RegDataType, REG_SIZE> reg;
    std::array<RobIDType, REG_SIZE> rename;

public:
    std::pair<RegDataType,RobIDType> get(RegIDType id){
        return {reg[id],rename[id]};
    }
    void preset(RegIDType id, RobIDType rob_id){
        rename[id] = rob_id;
    }

    void flush(){
        for(int i = 0; i < REG_SIZE; i++){
            if(rename[i] != 0){
                rename[i] = 0;
            }
        }
    }

    void fill(RobIDType rob_id, RegDataType value){
        for(int i = 0; i < REG_SIZE; i++){
            if(rename[i] == rob_id){
                reg[i] = value;
            }
        }
    }
};