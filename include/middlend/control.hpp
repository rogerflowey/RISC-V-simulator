#pragma once

#include "constants.hpp"
#include "frontend/decoder.hpp"
#include "utils/bus.hpp"

struct FetchedInstruction {
    Instruction ins;
    RegDataType v_rs1=0;
    RegDataType v_rs2=0;
    RobIDType q_rs1=0;
    RobIDType q_rs2=0;
};

class Control{
    Channel<Instruction> ins_channel;

}