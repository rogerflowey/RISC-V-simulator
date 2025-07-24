# RISC-V-simulator

This is a C++ version RISK-V simulator, using Tomasulo.

## Architecture

Mostly same as the standard Tomasulo.
Differences are:

 - Rather than the conventional Units->CDB->ROB, the simulator uses Units->ROB->CDB to avoid the arbritator of CDB. So now register also listen to CDB for the data;
 - For simplicity, we use a in-order memory and a hybrid LSB: LSB is a FIFO queue, load is executed when reaches head, but store will wait until it is committed.