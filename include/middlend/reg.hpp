#pragma once

#include "constants.hpp"
#include "utils/port.hpp" // The generic ReadPort/WritePort templates
#include "utils/clock.hpp"
#include "logger.hpp"

#include <array>
#include <vector>
#include <utility>


struct PresetRequest {
    RegIDType reg_id;
    RobIDType rob_id;
};

struct FillRequest {
    RegIDType reg_id;
    RobIDType rob_id;
    RegDataType value;
};


class RegisterFile {
private:
    std::array<RegDataType, REG_SIZE> reg{}; 
    std::array<RobIDType, REG_SIZE> rename{};

    std::vector<WritePort<PresetRequest>*> preset_ports;
    std::vector<WritePort<FillRequest>*> fill_ports;

public:
    RegisterFile() {
        reg.fill(0);
        rename.fill(0);
        Clock::getInstance().subscribe([this] { this->update_state(); }, FALLING);
    }

    ReadPort<RegIDType, std::pair<RegDataType, RobIDType>> create_get_port() {
        return ReadPort<RegIDType, std::pair<RegDataType, RobIDType>>(
            [this](RegIDType id) { return this->_get(id); }
        );
    }
    WritePort<PresetRequest> create_preset_port() {
        auto* port = new WritePort<PresetRequest>(); // Simplified memory management
        preset_ports.push_back(port);
        return *port;
    }
    WritePort<FillRequest> create_fill_port() {
        auto* port = new WritePort<FillRequest>(); // Simplified memory management
        fill_ports.push_back(port);
        return *port;
    }

    void flush() {
        logger.Info("Flushing Register Alias Table.");
        rename.fill(0);
    }

    //just for testing purposes
    const std::array<RegDataType, REG_SIZE>& get_snapshot() const {
        return reg;
    }

private:
    void update_state() {
        for (auto* port : preset_ports) {
            if (auto req = port->consume()) {
                _preset(req->reg_id, req->rob_id);
            }
        }
        for (auto* port : fill_ports) {
            if (auto req = port->consume()) {
                _fill(req->rob_id, req->reg_id, req->value);
            }
        }
    }
    std::pair<RegDataType, RobIDType> _get(RegIDType id) {
        logger.With("reg", static_cast<int>(id)).With("value", reg[id]).With("ROB_id", rename[id]).Info("RegisterFile read port accessed.");
        return {reg[id], rename[id]};
    }

    void _preset(RegIDType id, RobIDType rob_id) {
        logger.With("reg", static_cast<int>(id)).With("ROB_id", rob_id).Info("RAT preset executed on falling edge.");
        rename[id] = rob_id;
    }

    void _fill(RobIDType rob_id, RegIDType reg_id, RegDataType value) {
        if (reg_id == 0) {
            return;
        }
        logger.With("reg", static_cast<int>(reg_id)).With("value", value).With("ROB_id", rob_id).Info("ARF fill executed on falling edge.");
        reg[reg_id] = value;
        if (rename[reg_id] == rob_id) {
            logger.With("reg", static_cast<int>(reg_id)).With("ROB_id", rob_id).Info("Corresponding RAT entry cleared.");
            rename[reg_id] = 0;
        }
    }
};