#pragma once

#include "utils/clock.hpp" 
#include <functional>
#include <stdexcept>

/**
 * @class ReadPort
 * @brief A generic, single-use, read-only port for synchronous data access.
 *
 * @details This class models a physical read port on a hardware module (a "Holder").
 * It provides a synchronous, combinational "pull" interface for a "Worker" module.
 * The port is designed to be used exactly once per clock cycle, mimicking the physical
 * limitation of a single hardware read port. An internal flag, reset by the global
 * clock, enforces this rule at runtime.
 *
 * The core logic is provided via a `std::function` at construction, allowing this
 * generic port to be configured to perform any specific read action on its owning Holder.
 *
 * @tparam Input The data type of the query or address sent to the port (e.g., an ID, an index).
 * @tparam Output The data type of the result returned by the port.
 */
template<typename Input, typename Output>
class ReadPort {
private:
    /// @brief The function object that encapsulates the actual read logic from the Holder.
    std::function<Output(Input)> func;

    /// @brief A flag to enforce the single-use-per-cycle hardware limitation.
    bool triggered = false;

public:
    /**
     * @brief Constructs a ReadPort.
     * @param func A function (typically a lambda) that implements the read logic.
     *             This function will be called when the port's `read()` method is invoked.
     */
    ReadPort(std::function<Output(Input)> func) : func(func) {
        // Subscribe to the global clock to automatically reset the port's state
        // at the beginning of every cycle.
        Clock::getInstance().subscribe([this] { this->triggered = false; });
    }

    /**
     * @brief Executes a read operation through the port.
     * @details This method should be called by a Worker on the RISING edge of the clock.
     * It immediately executes the provided function and returns the result, modeling
     * a combinational read path.
     *
     * @param input The query or address for the read operation.
     * @return The result of the read operation.
     * @throws std::runtime_error if the port has already been used in the current clock cycle,
     *         simulating a structural hazard where a single physical port is requested twice.
     */
    Output read(Input input) {
        if (triggered) {
            throw std::runtime_error("ReadPort already triggered in this clock cycle");
        }
        triggered = true;
        return func(input);
    }
};
#pragma once

#include <optional>
#include <stdexcept>

/**
 * @class WritePort
 * @brief A generic, buffered, single-entry port for asynchronous data writes.
 *
 * @details This class models a physical write port on a hardware module (a "Holder"),
 * specifically one that uses a pipeline register to defer the write operation.
 * It provides an asynchronous "push" interface. A "Worker" module pushes data into
 * the port's buffer on the RISING edge of the clock. The owning "Holder" module then
 * consumes this data from the buffer on the FALLING edge, completing the write.
 *
 * This two-phase process (push-then-consume) perfectly models the standard hardware
 * pattern for writing to stateful elements without creating race conditions.
 *
 * @tparam DataType The data type of the value being written.
 */
template<typename DataType>
class WritePort {
private:
    /// @brief A single-entry buffer that holds the data between the rising and falling clock edges.
    std::optional<DataType> buffer;

public:
    /**
     * @brief Default constructor for the WritePort.
     */
    WritePort() = default;

    /**
     * @brief Checks if the port is ready to accept a new write request.
     * @details A Worker should call this on the RISING edge before pushing.
     * @return `true` if the port's buffer is empty (i.e., the Holder consumed the
     *         previous cycle's data), `false` otherwise. A `false` return indicates
     *         a structural hazard or back-pressure.
     */
    bool can_push() const {
        return !buffer.has_value();
    }

    /**
     * @brief Pushes data into the port's buffer.
     * @details This method should be called by a Worker on the RISING edge. It places
     * the data into the internal buffer to be processed later by the Holder.
     *
     * @param data The data to be written.
     * @throws std::runtime_error if the buffer already contains data, indicating the
     *         caller did not respect the `can_push()` check.
     */
    void push(DataType data) {
        if (buffer.has_value()) {
            throw std::runtime_error("WritePort buffer already contains data");
        }
        buffer = data;
    }

    /**
     * @brief Consumes the data from the port's buffer.
     * @details This method should be called by the owning Holder on the FALLING edge.
     * It retrieves the data (if any) that was pushed during the rising edge and
     * simultaneously clears the buffer, making the port ready for the next cycle.
     *
     * @return An `std::optional` containing the data if a write occurred, or `std::nullopt`
     *         if no data was pushed to the port this cycle.
     */
    std::optional<DataType> consume() {
        if (!buffer.has_value()) {
            return std::nullopt;
        }
        // Create a copy to return, then clear the buffer.
        std::optional<DataType> data_to_return = buffer;
        buffer.reset();
        return data_to_return;
    }
};