#pragma once

#include "constants.hpp"
#include "utils/buffered.hpp"

class CommonDataBus{
    Buffered<RobIDType> id;
    Buffered<RegType> value;
public:
    void set(RobIDType id,RegType value){
        this->id<=id;
        this->value<=value;
    }
    std::pair<RobIDType,RegType> get(){
        return {*id,*value};
    }
};