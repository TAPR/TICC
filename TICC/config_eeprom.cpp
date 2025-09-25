// config_eeprom.cpp -- EEPROM read/write operations

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Portions Copyright David McQuate WA8YWQ 2016
// Licensed under BSD 2-clause license

#include <stdint.h>
#include <EEPROM.h>
#include <Arduino.h>
#include "config.h"

// External variables
extern config_t config;
extern char SER_NUM[17];

// Write default configuration to EEPROM at specified offset
void eeprom_write_config_default(uint16_t offset) {
  struct config_t x = defaultConfig();
  EEPROM_writeAnything(offset, x);
}

// Clear entire EEPROM (write 0xFF to all locations)
void eeprom_clear() {
  // write 0xFF (factory default) to entire eeprom area
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0xFF);
  }
}

// Write current config to EEPROM
void eeprom_write_config() {
  EEPROM_writeAnything(CONFIG_START, config);
}

// Read config from EEPROM
void eeprom_read_config() {
  EEPROM_readAnything(CONFIG_START, config);
}

// Get serial number from EEPROM
void get_serial_number() {
  // Serial number is 8 bytes. On first run,
  // check location and if not found, use random()
  // to generate. If found, just read, format as string,
  // and set config.SER_NUM
  
  int32_t x, y;  // 2 longs because sprintf can't handle uint64
  
  EEPROM_readAnything(SER_NUM_START, x);
  EEPROM_readAnything(SER_NUM_START + 4, y);

  // New Arduino has all EEPROM set to 0xFF; example
  // clear routine sets to 0x00. Test for both
  // If no serial number, make one
  if (((x == 0xFFFFFFFF) && (y == 0xFFFFFFFF)) ||
      ((x == 0x00000000) && (y == 0x00000000))) {
    Serial.println("No serial number found... making one");
    randomSeed(analogRead(A0));  // seed with noise from A0
    x = random(0xFFFF);
    randomSeed(analogRead(A3));  // seed with noise from A3
    y = random(0xFFFF);
    EEPROM_writeAnything(SER_NUM_START, x);
    EEPROM_writeAnything(SER_NUM_START + 4, y);
    sprintf(SER_NUM, "%04lX%04lX", x, y); 
    Serial.print("Serial Number: ");
    Serial.println(SER_NUM);
    delay(7500);
  }
  sprintf(SER_NUM, "%04lX%04lX", x, y);
}
