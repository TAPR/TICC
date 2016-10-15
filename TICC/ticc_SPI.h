#ifndef TICC_SPI_H
#define TICC_SPI_H

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.79 14 October 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license



// Chip properties:
// Most Significant Bit is clocked first (MSBFIRST)
// clock is low when idle
// data is clocked on the rising edge of the clock (seems to be SPI_MODE0)
// max clock speed: 20 mHz

#define SPI_SPEED         (int32_t)  20000000 // 20MHz maximum

byte SPI_readReg8(byte address, byte CSB);

uint32_t SPI_readReg24(byte address, byte CSB);

void SPI_write(byte address, byte value, byte CSB);

#endif	/* TICC_SPI_H */
