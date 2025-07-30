#pragma once

#include "constants.hpp"
#include "logger.hpp"

#include <utility>
#include <array>




class RegisterFile{
    std::array<RegDataType, REG_SIZE> reg;
    std::array<RobIDType, REG_SIZE> rename;

public:
    std::pair<RegDataType,RobIDType> get(RegIDType id){
        logger.With("reg",static_cast<int>(id)).With("value",reg[id]).With("ROB_id",rename[id]).Info("Getting register value and RAT entry");
        return {reg[id],rename[id]};
    }
    void preset(RegIDType id, RobIDType rob_id){
        logger.With("reg",static_cast<int>(id)).With("ROB_id", rob_id).Info("Setting RAT");
        rename[id] = rob_id;
    }

    void flush(){
        logger.Info("Flushing RAT");
        for(int i = 0; i < REG_SIZE; i++){
            if(rename[i] != 0){
                rename[i] = 0;
            }
        }
    }

    void fill(RobIDType rob_id, RegIDType reg_id, RegDataType value){
        if(reg_id==0){
            return;
        }
        logger.With("reg",static_cast<int>(reg_id)).With("value",value).With("ROB_id",rob_id).Info("Filling register");
        reg[reg_id] = value;
        if(rename[reg_id] == rob_id){
            logger.With("reg",static_cast<int>(reg_id)).With("ROB_id",rob_id).Info("Clearing RAT entry");
            rename[reg_id] = 0;
        }
    }
};