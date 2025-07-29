#pragma once

#include "constants.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"

class PCLogic {
    PCType pc;
    // Input
    Channel<PCType>& prediction_c;
    Channel<PCType>& flush_c;
    // Output
    Channel<PCType>& final_pc;

public:
    PCLogic(Channel<PCType>& pred, Channel<PCType>& flush, Channel<PCType>& final)
        : pc(0), prediction_c(pred), flush_c(flush), final_pc(final) {
        Clock::getInstance().subscribe([this]{ this->work(); });
    }
    void work() {
        if (!final_pc.can_send()) {
            return; // STALL
        }
        if (auto flush_result = flush_c.receive()) {
            pc = flush_result.value();
        }
        else if (auto pred_result = prediction_c.receive()) {
            pc = pred_result.value();
        }
        final_pc.send(pc);

        pc += 4;
    }
};