#include <iostream>
#include <cassert>

#include "utils/clock.hpp"
#include "utils/bus.hpp"


void test_channel_basic_flow() {
    std::cout << "Running: " << __func__ << std::endl;
    Clock::getInstance().reset();
    Channel<int> channel;

    // Initial state: channel is empty
    assert(!channel.peek().has_value());
    assert(!channel.receive().has_value());

    // Send data, but before clock tick, it's not available
    assert(channel.send(42) == true);
    assert(!channel.peek().has_value());

    // 1. Clock tick (FALLING edge latches data from writer to reader)
    Clock::getInstance().tick();

    // Data is now available
    assert(channel.peek().has_value());
    assert(channel.peek().value() == 42);

    // Peek again, should still be there
    assert(channel.peek().value() == 42);

    // Receive the data
    auto received_data = channel.receive();
    assert(received_data.has_value());
    assert(received_data.value() == 42);
    // After receive(), data is marked as consumed but still in the slot until next tick
    assert(channel.peek().has_value()); 
    assert(channel.peek().value() == 42);

    // 2. Clock tick (FALLING edge clears the consumed data)
    Clock::getInstance().tick();

    // Channel should now be empty
    assert(!channel.peek().has_value());
    assert(!channel.receive().has_value());
    std::cout << "PASSED" << std::endl;
}

void test_channel_backpressure() {
    std::cout << "Running: " << __func__ << std::endl;
    Clock::getInstance().reset();
    Channel<std::string> channel;

    // Send once, should succeed
    assert(channel.send("hello") == true);

    // Send again before a tick, should fail (back-pressure)
    assert(channel.send("world") == false);

    // 1. Clock tick (latches "hello")
    Clock::getInstance().tick();

    // Now the writer slot is free, so we can send "world"
    assert(channel.send("world") == true);

    // The reader should see the first value, "hello"
    assert(channel.peek().has_value());
    assert(channel.peek().value() == "hello");
    channel.receive(); // Consume "hello"

    // 2. Clock tick (clears "hello", latches "world")
    Clock::getInstance().tick();

    // The reader should now see the second value, "world"
    assert(channel.peek().has_value());
    assert(channel.peek().value() == "world");
    std::cout << "PASSED" << std::endl;
}

void test_bus_basic_flow() {
    std::cout << "Running: " << __func__ << std::endl;
    Clock::getInstance().reset();
    Bus<int> bus;

    // Time 0: Send data
    assert(bus.send(101) == true);
    // Data is not yet available
    assert(!bus.get().has_value());
    assert(Clock::getInstance().getTime() == 0);

    // Time 1: Clock tick
    // RISING: Bus tries to receive, but reader slot is empty.
    // FALLING: Channel latches 101 into reader slot.
    Clock::getInstance().tick();
    assert(Clock::getInstance().getTime() == 1);

    // Data sent at T=0 is now available at T=1
    assert(bus.get().has_value());
    assert(bus.get().value() == 101);

    // Time 2: Clock tick
    // RISING: Bus automatically calls receive(), consuming 101.
    // FALLING: Channel sees data was consumed and clears the reader slot.
    Clock::getInstance().tick();
    assert(Clock::getInstance().getTime() == 2);

    // Data is gone because it was consumed and a cycle has passed
    assert(!bus.get().has_value());
    std::cout << "PASSED" << std::endl;
}

void test_bus_pipelined_data() {
    std::cout << "Running: " << __func__ << std::endl;
    Clock::getInstance().reset();
    Bus<int> bus;

    // Time 0:
    assert(Clock::getInstance().getTime() == 0);
    assert(bus.send(10) == true);
    assert(!bus.get().has_value()); // Nothing to get yet

    // Time 1:
    Clock::getInstance().tick();
    assert(Clock::getInstance().getTime() == 1);
    assert(bus.get().has_value() && bus.get().value() == 10); // Get data from T=0
    assert(bus.send(20) == true); // Send next data

    // Time 2:
    Clock::getInstance().tick();
    assert(Clock::getInstance().getTime() == 2);
    assert(bus.get().has_value() && bus.get().value() == 20); // Get data from T=1
    assert(bus.send(30) == true); // Send next data

    // Time 3:
    Clock::getInstance().tick();
    assert(Clock::getInstance().getTime() == 3);
    assert(bus.get().has_value() && bus.get().value() == 30); // Get data from T=2
    // Don't send anything new

    // Time 4:
    Clock::getInstance().tick();
    assert(Clock::getInstance().getTime() == 4);
    // Nothing was sent at T=3, so bus is now empty
    assert(!bus.get().has_value()); 

    std::cout << "PASSED" << std::endl;
}


int main() {
    test_channel_basic_flow();
    std::cout << "---------------------" << std::endl;
    test_channel_backpressure();
    std::cout << "---------------------" << std::endl;
    test_bus_basic_flow();
    std::cout << "---------------------" << std::endl;
    test_bus_pipelined_data();
    std::cout << "---------------------" << std::endl;

    std::cout << "\nAll tests passed successfully!" << std::endl;

    return 0;
}