#pragma once

#include "backend/cdb.hpp"
#include "constants.hpp"
#include "middlend/reg.hpp"
#include "backend/units/memory.hpp"
#include "backend/units/mob.hpp"
#include "utils/bus.hpp"
#include "utils/hive.hpp"


struct MemoryIns{
    MemoryRequest req;
    MemAddrType v_rs1;
    RobIDType q_rs1;
    RegIDType rs1;

    RegIDType rd;
    
    MemAddrType v_rs2;
    RobIDType q_rs2;
    RegIDType rs2;

    ImmType imm; 
};


class MemoryRS{
    hive<MemoryIns, LSB_SIZE> buffer;
    Channel<MemoryIns> ins_channel;

    CommonDataBus& cdb;
    MemoryOrderBuffer& mob;
    RegisterFile& reg;
public:
    MemoryRS(CommonDataBus& cdb, MemoryOrderBuffer& mob, RegisterFile& reg) : cdb(cdb), mob(mob), reg(reg) {
        Clock::getInstance().subscribe([this]{ this->work(); });
    }

    void work(){
        // phase 1: Process incoming instructions
        if(!buffer.full()){
            auto result = ins_channel.receive();
            if(result){
                buffer.insert(*result);
            }
        }

        // phase 2: listen CDB to update entries
        auto cdb_result = cdb.get();
        if(cdb_result){
            for(auto it = buffer.begin(); it != buffer.end(); ++it){
                if(it->q_rs1 == cdb_result->rob_id){
                    it->v_rs1 = cdb_result->data;
                    it->q_rs1 = 0;
                }
                if(it->q_rs2 == cdb_result->rob_id){
                    it->v_rs2 = cdb_result->data;
                    it->q_rs2 = 0;
                }
            }
        }

        // phase 3: send memory requests
        for(auto it = buffer.begin(); it != buffer.end(); ++it){
            if(it->q_rs1 == 0 && it->q_rs2 == 0){
                
                buffer.erase(it);
                break;
            }
        }

    }
};