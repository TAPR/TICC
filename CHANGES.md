# TICC Change Log

## Version 202510xx

**Major Release** - Reworked configuration menu system and internal
improvements; much rewritten code

### Breaking Changes
- **Many Configuration Menu Changes**: The configuration menu structure
  has changed and most parameter IDs are now different.  Also, a few
  parameter values have changed -- e.g., SYNC_MODE is now
  [P]rimary/[S]econdary instead of [M]aster/[C]lient.
- **New EEPROM Layout**: The layout of configuration values stored in
  EEPROM has been updated from version 10 to version 12.  On first
  execution the EEPROM will be updated to the new version.  All config 
  values will be reset to defaults, but the board serial number will 
  be retained.

### New Features
- ** New Modes**:
  - **Channel-Paired Timestamp**: Attempts to sort dual-channel output
    for channel order (e.g., chA/chB/chA/chB) to meet TimeLab multi-channel
    requirements.
  - **Strict Order Timestamp**: Attempts to order dual-channel output
    to maintain strict timestamp order (e.g., always increasing timestamp
    regardless of channel order)
  - **Immediate Timestamp**: This is the original timestamp mode; each
    timestamp is printed as soon as it is ready, regardless of channel or
    monotonic order.
  - **Binary Timestamp**: Outputs binary data to achieve >1000
    measurements/second on one channel
  
- **Enhanced Configuration Menus and New Options**: 
  - **Multiple commands per line**: Allows simplified scripting
  - **Timestamp Wraparound Configuration**: Timestamp wraparound can now be
    enabled in the startup configuration menu (e.g., wrap at 100 seconds)
  - **Configurable Precision**: The number of decimal places can now be set
    from the configuration menus
  - **Configurable Baud Rate**: Allows 230400 baud for maximum throughput
- **Updated Documentation**: TICC User Guide completely rewritten and much
  expanded

### Improvements
- **Timestamp Calculation Optimized**: Removed much 64 bit math to increase
  throughput; raw calculation speed is >1250 measurements/second in one channel
- **Optimized Print Routines**: Reworked printing to optimize 64 bit variable
  output (an 8 bit Arduino weak point).  Now can output normal-format timestamps
  at 565 Hz.
- **Extended Timestamp Range**: Timestamp calculation modified so that
  overflow will not happen for about 68 years
- **Print Function Fixes**: Resolved display errors that occurred when
  timestamps became very large
- **Interrupt Processing**: Fixed fundamental problems processing TDC7200
  chip interrupts, removing ugly hacks that affected maximum measurement
  rate

---

## Version 20200412.1

**Significant Release** - Output resolution changes and configuration
improvements

### Breaking Changes
- **Output Resolution**: Changed from 12 to 11 decimal places (last
  picosecond digit was pure noise)

### New Features
- **Timestamp Wraparound**: Configurable timestamp wraparound value via
  config menu
  - Default: 0 (no wraparound, timestamps increase indefinitely)
  - Setting 1: wrap at 10 seconds
  - Setting 2: wrap at 100 seconds
  - Setting 3: wrap at 1000 seconds, etc.
- **Channel Naming**: New "I" command allows custom channel names instead
  of default "A" and "B"
- **Terminology Update**: "Master/Slave" changed to "Primary/Secondary"

### Configuration Changes
- **Menu Restructure**: Commands now identified by sequential letters (ran
  out of useful mnemonics)
- **Configurable Precision**: Number of decimal places set via PLACES
  define in config.h

### Technical Notes

#### Floating Point Precision
Most computer programs process non-integer numeric data using floating
point operations with ~15.9 digits of precision. When the combined integer
and fractional places exceed 16 digits, accuracy of the last decimal
places is reduced.

This effect becomes apparent on long timestamp series of low-noise data:
- With 12 decimal places representing single picoseconds, accuracy loss
  appears when integer part exceeds 999 seconds
- Effect usually becomes noticeable at 10,000+ seconds
- Effect increases as total digit count increases

**Remediation in this version:**
1. **Reduced Precision**: Fractional part now 11 places (10 ps resolution)
   instead of 12
2. **Timestamp Wrapping**: Integer part can be set to wrap at
   10/100/1000+ seconds to constrain digit count
3. **Software Compatibility**: Most time analysis software can be
   configured to compensate for wraps

---

## Version History

| Version | Date | Type | Key Changes |
|---------|------|------|-------------|
| 2025090x.1 | 2025-09-xx | Major | Configuration overhaul, timestamp
improvements, performance fixes |
| 20200412.1 | 2020-04-12 | Significant | Precision changes, wraparound
feature, menu restructure |

---

*For detailed technical information, see the TICC User Guide and source
code documentation.*
