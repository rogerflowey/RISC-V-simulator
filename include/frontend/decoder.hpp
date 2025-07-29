#pragma once
#include "constants.hpp"
#include "frontend/fetcher.hpp"
#include "frontend/predictor.hpp"
#include "instruction.hpp"
#include "utils/bus.hpp"
#include "utils/clock.hpp"

class Decoder {

  // input
  Channel<FetchResult> &input_c;
  // output
  Channel<Instruction> &output_c;
  Channel<PCType> &pc_pred_c;

  // Flush signal buses
  Bus<bool> &flush_bus;         
  Bus<bool> &frontend_flush_bus;

  // Internal components
  Predictor predictor;

public:
  Decoder(Channel<Instruction> &output_channel,
          Channel<FetchResult> &input_channel,
          Channel<PCType> &pc_pred_channel,
          Bus<bool> &flush_signal_from_execute,
          Bus<bool> &flush_signal_to_frontend)
      : input_c(input_channel), output_c(output_channel),
        pc_pred_c(pc_pred_channel), flush_bus(flush_signal_from_execute),
        frontend_flush_bus(flush_signal_to_frontend) {
    Clock::getInstance().subscribe([this] { this->work(); });
  }

  void work() {
    if (flush_bus.get() || frontend_flush_bus.get()) {
      flush();
      return;
    }
    if (!output_c.can_send()) {
      return;
    }
    if (auto fetch_result = input_c.receive()) {
      Instruction decoded_inst =
          decode(fetch_result->instruction, fetch_result->pc);
      handle_control_flow(decoded_inst);
      output_c.send(decoded_inst);
    }
  }

  void update_predictor(PCType pc, bool actually_taken) {
    predictor.update(pc, actually_taken);
  }

private:
  // This private method performs the flush action on this stage.
  void flush() {
    input_c.receive();
  }

  void handle_control_flow(Instruction &inst) {
    bool redirect_pc = false;
    PCType target_pc = 0;

    switch (inst.op) {
    case OpType::BEQ:
    case OpType::BNE:
    case OpType::BLT:
    case OpType::BGE:
    case OpType::BLTU:
    case OpType::BGEU:
      inst.is_branch = true;
      inst.predicted_taken = predictor.predict(inst.pc);
      if (inst.predicted_taken) {
        redirect_pc = true;
        target_pc = inst.pc + inst.imm;
      }
      break;

    case OpType::JAL:
      redirect_pc = true;
      target_pc = inst.pc + inst.imm;
      break;

    case OpType::JALR:
      break;

    default:
      break;
    }

    if (redirect_pc) {
      pc_pred_c.send(target_pc);
      frontend_flush_bus.send(true);
    }
  }

  Instruction decode(uint32_t instruction_word, PCType current_pc) {
    Instruction decoded_inst;
    decoded_inst.pc = current_pc;

    const uint32_t opcode = instruction_word & 0x7F;
    const uint32_t rd = (instruction_word >> 7) & 0x1F;
    const uint32_t funct3 = (instruction_word >> 12) & 0x7;
    const uint32_t rs1 = (instruction_word >> 15) & 0x1F;
    const uint32_t rs2 = (instruction_word >> 20) & 0x1F;
    const uint32_t funct7 = (instruction_word >> 25) & 0x7F;

    switch (opcode) {
    case 0b0110111: // LUI
      decoded_inst.op = OpType::LUI;
      decoded_inst.rd = rd;
      decoded_inst.imm = instruction_word & 0xFFFFF000;
      break;
    case 0b0010111: // AUIPC
      decoded_inst.op = OpType::AUIPC;
      decoded_inst.rd = rd;
      decoded_inst.imm = instruction_word & 0xFFFFF000;
      break;

    case 0b1101111: // JAL
      decoded_inst.op = OpType::JAL;
      decoded_inst.rd = rd;
      {
        uint32_t imm20 = (instruction_word >> 31) & 1;
        uint32_t imm10_1 = (instruction_word >> 21) & 0x3FF;
        uint32_t imm11 = (instruction_word >> 20) & 1;
        uint32_t imm19_12 = (instruction_word >> 12) & 0xFF;
        uint32_t imm_val =
            (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
        decoded_inst.imm = static_cast<int32_t>(imm_val << 11) >> 11;
      }
      break;

    case 0b1100111: // JALR
      decoded_inst.op = OpType::JALR;
      decoded_inst.rd = rd;
      decoded_inst.rs1 = rs1;
      decoded_inst.imm = static_cast<int32_t>(instruction_word) >> 20;
      break;

    case 0b1100011: // BEQ, BNE, BLT, BGE, BLTU, BGEU
      decoded_inst.rs1 = rs1;
      decoded_inst.rs2 = rs2;
      {
        uint32_t imm12 = (instruction_word >> 31) & 1;
        uint32_t imm10_5 = (instruction_word >> 25) & 0x3F;
        uint32_t imm4_1 = (instruction_word >> 8) & 0xF;
        uint32_t imm11 = (instruction_word >> 7) & 1;
        uint32_t imm_val =
            (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
        decoded_inst.imm = static_cast<int32_t>(imm_val << 19) >> 19;
      }
      switch (funct3) {
      case 0b000:
        decoded_inst.op = OpType::BEQ;
        break;
      case 0b001:
        decoded_inst.op = OpType::BNE;
        break;
      case 0b100:
        decoded_inst.op = OpType::BLT;
        break;
      case 0b101:
        decoded_inst.op = OpType::BGE;
        break;
      case 0b110:
        decoded_inst.op = OpType::BLTU;
        break;
      case 0b111:
        decoded_inst.op = OpType::BGEU;
        break;
      default:
        decoded_inst.op = OpType::INVALID;
        break;
      }
      break;

    case 0b0000011: // LB, LH, LW, LBU, LHU
      decoded_inst.rd = rd;
      decoded_inst.rs1 = rs1;
      decoded_inst.imm = static_cast<int32_t>(instruction_word) >> 20;
      switch (funct3) {
      case 0b000:
        decoded_inst.op = OpType::LB;
        break;
      case 0b001:
        decoded_inst.op = OpType::LH;
        break;
      case 0b010:
        decoded_inst.op = OpType::LW;
        break;
      case 0b100:
        decoded_inst.op = OpType::LBU;
        break;
      case 0b101:
        decoded_inst.op = OpType::LHU;
        break;
      default:
        decoded_inst.op = OpType::INVALID;
        break;
      }
      break;

    case 0b0100011: // SB, SH, SW
      decoded_inst.rs1 = rs1;
      decoded_inst.rs2 = rs2;
      {
        uint32_t imm11_5 = (instruction_word >> 25) & 0x7F;
        uint32_t imm4_0 = (instruction_word >> 7) & 0x1F;
        uint32_t imm_val = (imm11_5 << 5) | imm4_0;
        decoded_inst.imm = static_cast<int32_t>(imm_val << 20) >> 20;
      }
      switch (funct3) {
      case 0b000:
        decoded_inst.op = OpType::SB;
        break;
      case 0b001:
        decoded_inst.op = OpType::SH;
        break;
      case 0b010:
        decoded_inst.op = OpType::SW;
        break;
      default:
        decoded_inst.op = OpType::INVALID;
        break;
      }
      break;

    case 0b0010011: // ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
      decoded_inst.rd = rd;
      decoded_inst.rs1 = rs1;
      switch (funct3) {
      case 0b000:
        decoded_inst.op = OpType::ADDI;
        break;
      case 0b010:
        decoded_inst.op = OpType::SLTI;
        break;
      case 0b011:
        decoded_inst.op = OpType::SLTIU;
        break;
      case 0b100:
        decoded_inst.op = OpType::XORI;
        break;
      case 0b110:
        decoded_inst.op = OpType::ORI;
        break;
      case 0b111:
        decoded_inst.op = OpType::ANDI;
        break;
      case 0b001: // SLLI
        decoded_inst.op = OpType::SLLI;
        break;
      case 0b101: // SRLI / SRAI
        if (funct7 == 0b0100000) {
          decoded_inst.op = OpType::SRAI;
        } else {
          decoded_inst.op = OpType::SRLI;
        }
        break;
      default:
        decoded_inst.op = OpType::INVALID;
        break;
      }
      // For shifts, the immediate is just the 5-bit shamt.
      // For others, it's a 12-bit sign-extended value.
      if (funct3 == 0b001 || funct3 == 0b101) {
        decoded_inst.imm = rs2; // shamt is in the rs2 field for I-type shifts
      } else {
        decoded_inst.imm = static_cast<int32_t>(instruction_word) >> 20;
      }
      break;

    case 0b0110011: // ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
      decoded_inst.rd = rd;
      decoded_inst.rs1 = rs1;
      decoded_inst.rs2 = rs2;
      switch (funct3) {
      case 0b000: // ADD / SUB
        if (funct7 == 0b0100000) {
          decoded_inst.op = OpType::SUB;
        } else { // funct7 == 0b0000000
          decoded_inst.op = OpType::ADD;
        }
        break;
      case 0b001:
        decoded_inst.op = OpType::SLL;
        break;
      case 0b010:
        decoded_inst.op = OpType::SLT;
        break;
      case 0b011:
        decoded_inst.op = OpType::SLTU;
        break;
      case 0b100:
        decoded_inst.op = OpType::XOR;
        break;
      case 0b101: // SRL / SRA
        if (funct7 == 0b0100000) {
          decoded_inst.op = OpType::SRA;
        } else { // funct7 == 0b0000000
          decoded_inst.op = OpType::SRL;
        }
        break;
      case 0b110:
        decoded_inst.op = OpType::OR;
        break;
      case 0b111:
        decoded_inst.op = OpType::AND;
        break;
      default:
        decoded_inst.op = OpType::INVALID;
        break;
      }
      break;

    default:
      decoded_inst.op = OpType::INVALID;
      break;
    }

    return decoded_inst;
  }
};