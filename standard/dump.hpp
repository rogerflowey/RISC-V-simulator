#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>

namespace norb {

    // Helper function to format a number as a hex string
    template <typename T>
    std::string hex(T val) {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::setw(sizeof(T) * 2) << std::setfill('0') << val;
        return oss.str();
    }

    // Helper function to pad a number with leading zeros
    template <typename T>
    std::string pad_with_zero(T val, int width) {
        std::ostringstream oss;
        oss << std::setw(width) << std::setfill('0') << val;
        return oss.str();
    }

    template <size_t reg_count_, typename RegType_ = uint32_t>
    class RegisterDumper {
    private:
        std::ofstream file_;
        int line_number_;

    public:
        RegisterDumper(const std::string &filename) : line_number_(0) {
            file_.open(filename, std::ios::trunc);
            if (!file_.is_open()) {
                throw std::runtime_error("Failed to open file for register dumping: " + filename);
            }
        }

        ~RegisterDumper() {
            if (file_.is_open()) {
                file_.close();
            }
        }

        void dump(uint32_t pc_at_commit, const std::array<RegType_, reg_count_> reg_snapshot) {
            dump_impl(pc_at_commit, reg_snapshot);
        }

    private:
        // CORRECTED: Matches your requested output format
        void dump_impl(uint32_t pc_at_commit, const std::array<RegType_, reg_count_> reg_snapshot) {
            std::ostringstream oss;
            oss << "[" << norb::pad_with_zero(++line_number_, 4) << "] ";
            oss << norb::hex(pc_at_commit) << " | ";
            for (size_t i = 0; i < reg_count_; ++i) {
                const auto reg_value = reg_snapshot[i];
                oss << "R" << i << "(";
                if (reg_value == 0) {
                    oss << "0";
                } else {
                    // Show both decimal and hex for non-zero values
                    oss << std::dec << reg_value << "=" << norb::hex(reg_value);
                }
                oss << ")";
                if (i < reg_count_ - 1) oss << " ";
            }
            oss << "\n";

            file_ << oss.str();
            file_.flush();
        }
    };
}  // namespace norb