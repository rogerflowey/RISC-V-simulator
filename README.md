# RISC-V-simulator

This is a C++ version RISK-V simulator, using Tomasulo.

## Architecture

Mostly same as the standard Tomasulo.
Differences are:





# Development Log

 - Rather than the conventional Units->CDB->ROB, the simulator uses Units->ROB->CDB to avoid the arbritator of CDB. So now register also listen to CDB for the data;
 - Update: the above is abandoned. To solve arbitration, we use a two-stage channel, units each write to their individual channel, and CDB will choseone to receive.
 - For simplicity, we use a in-order memory and a hybrid LSB: LSB is a FIFO queue, load is executed when reaches head, but store will wait until it is committed.
 - Update: Instead of a single LSB, we seperate it into: a new LSB that only does work of a RS, and a MOB(memory order buffer), which handles the ordering of memory requests.The latter only receive fully filled memory request and do the above FIFO ordering(It will be responsible for resolve Store-load conflicts and be partially ordered in the future)
 - clarify: ROB instead of RS should be responsible for fetching value(and tags) from RF, so that Roll back can be handled more easily.