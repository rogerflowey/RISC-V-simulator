#pragma once

#include "constants.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"


class PCLogic{
    PCType pc;
    Channel<PCType> increment_c;
    Channel<PCType> prediction_c;
    Channel<PCType> flush_c;
    Channel<PCType> final_pc;
public:
    PCLogic(){
        pc = 0;
        Clock::getInstance().subscribe([this]{this->work();});
    }

    Channel<PCType> get_increment_channel() {
        return increment_c;
    }

    Channel<PCType> get_prediction_channel() {
        return prediction_c;
    }

    Channel<PCType> get_flush_channel() {
        return flush_c;
    }

    void work(){
        auto inc_result = increment_c.receive();
        auto pred_result = prediction_c.receive();
        auto flush_result = flush_c.receive();

        if(flush_result){
            pc = flush_result.value();
        } else if(pred_result){
            pc = pred_result.value();
        } else if(inc_result){
            pc = inc_result.value();
        }

        final_pc.send(pc);
    }
};