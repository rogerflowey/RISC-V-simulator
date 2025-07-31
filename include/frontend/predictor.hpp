#pragma once
#include "constants.hpp"
#include "logger.hpp"
#include <map>


enum STATUS {
    STRONG_NOT = 0,
    WEAK_NOT   = 1,
    WEAK_YES   = 2,
    STRONG_YES = 3 
};


class Predictor {
private:
    std::map<PCType, STATUS> prediction_table;

public:
    bool predict(PCType pc) {
        auto it = prediction_table.find(pc);

        if (it == prediction_table.end()) {
            logger.With("pc",pc).Info(("New entry,Not Taken"));
            return false; 
        }
        
        STATUS current_status = it->second;
        bool result = (current_status == WEAK_YES || current_status == STRONG_YES);
        logger.With("pc",pc).Info((result?"Taken":"Not Taken"));
        return result;
    }

    void update(PCType pc, bool actually_taken) {
        if (prediction_table.find(pc) == prediction_table.end()) {
            prediction_table[pc] = WEAK_NOT;
        }
        
        STATUS& current_status = prediction_table[pc];

        if (actually_taken) {
            if (current_status != STRONG_YES) {
                current_status = static_cast<STATUS>(current_status + 1);
            }
        } else {
            if (current_status != STRONG_NOT) {
                current_status = static_cast<STATUS>(current_status - 1);
            }
        }
    }
};