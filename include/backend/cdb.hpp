#pragma once

#include "constants.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"
#include "logger.hpp"

struct CDBResult{
    RobIDType rob_id;
    RegDataType data;
};

class CommonDataBus{
    Bus<CDBResult> out_bus;
    Bus<bool>& global_flush_bus;
    std::vector<Channel<CDBResult>*> in_channels;
public:
    CommonDataBus(Bus<bool>& global_flush_bus):global_flush_bus(global_flush_bus){
        Clock::getInstance().subscribe([this]() { this->work(); },RISING);
    }

    void connect(Channel<CDBResult>& in_bus){
        in_channels.push_back(&in_bus);
    }

    std::optional<CDBResult> get(){
        return out_bus.get();
    }

    void work(){
        if(global_flush_bus.get()) {
            logger.Info("Flushing CommonDataBus input channels");
            for(auto c:in_channels) {
                c->clear();
            }
            return;
        }
        auto start = Clock::getInstance().getTime()%in_channels.size();
        for(int i = 0; i < in_channels.size(); i++){
            int index = (start + i) % in_channels.size();
            auto result = in_channels[index]->receive();
            if(result){
                logger.With("ROB_ID", result->rob_id)
                      .With("Value", result->data)
                      .Info("Broadcasting result on CommonDataBus");
                out_bus.send(*result);
                break;
            }
        }
    }
};