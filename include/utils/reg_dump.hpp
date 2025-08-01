#pragma once

// This is a proposed protocol to dump the contents of registers.
// The real implementation is enabled by defining the ENABLE_REGISTER_DUMPER macro.
// Otherwise, a dummy no-op implementation is used with zero overhead.

#include <array>
#include <cstdint>
#include <string>

// The real implementation needs these headers.
#ifdef ENABLE_REGISTER_DUMPER
#include <fstream>
#include <sstream>
#include <stdexcept> // For std::runtime_error
#include "dump.hpp"
#endif

namespace norb {

#ifdef ENABLE_REGISTER_DUMPER

    // --- REAL IMPLEMENTATION ---
    // This version actively writes register snapshots to a file.
    // Use this for debugging.

    template <size_t reg_count_, typename RegType_ = uint32_t>
    class RegisterDumper {
    private:
        std::ofstream file_;
        int line_number_;

    public:
        RegisterDumper(const std::string &filename) : line_number_(0) {
            // Clear the file at bootup and keep it open
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
        void dump_impl(uint32_t pc_at_commit, const std::array<RegType_, reg_count_> reg_snapshot) {
            std::ostringstream oss;
            oss << "[" << norb::pad_with_zero(++line_number_, 4) << "] ";  // line number for each line
            oss << norb::hex(pc_at_commit) << " | ";  // PC at commit
            for (size_t i = 0; i < reg_count_; ++i) {
                const auto reg_value = reg_snapshot[i];
                if (reg_value == 0)
                    oss << "R" << i << "(" << 0 << ")";
                else
                    oss << "R" << i << "(" << reg_value << "=" << norb::hex(reg_value) << ")";
                if (i < reg_count_ - 1) oss << " ";
            }
            oss << "\n";

            // Write to the persistent file stream
            file_ << oss.str();
            file_.flush();  // Ensure data is written immediately
        }
    };

#else

    // --- DUMMY (NO-OP) IMPLEMENTATION ---
    // This version has the same interface but all methods are empty.
    // The compiler will optimize away any calls to it, resulting in zero runtime overhead.
    // Use this for performance-critical or release builds.

    template <size_t reg_count_, typename RegType_ = uint32_t>
    class RegisterDumper {
    public:
        // The constructor does nothing. The parameter name is commented out
        // to prevent "unused variable" warnings.
        RegisterDumper(const std::string& /*filename*/) {}

        // The destructor does nothing.
        ~RegisterDumper() {}

        // The dump method is a no-op and will be optimized out.
        void dump(uint32_t /*pc_at_commit*/, const std::array<RegType_, reg_count_>& /*reg_snapshot*/) {
            // Intentionally empty
        }
    };

#endif // ENABLE_REGISTER_DUMPER

}  // namespace norb