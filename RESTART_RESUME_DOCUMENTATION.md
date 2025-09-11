# TICC Restart/Resume Functionality Documentation

## Overview

This document describes the restart/resume functionality implemented in the TICC firmware to handle configuration changes during operation while maintaining timestamp continuity and proper state management.

## Problem Statement

The TICC firmware runs a continuous timestamp generation loop that:
- Uses a 100µs coarse clock (`PICcount`) incremented via hardware interrupt
- Processes measurements from two independent TDC7200 chips
- Generates timestamps by combining `PICcount` with TDC time-of-flight measurements

**Key Challenge**: When users enter configuration mode (via '#'), the main loop stops but:
- `PICcount` ISR continues running (preserving time continuity)
- TDC7200 chips may have partial measurements in progress
- Channel state becomes inconsistent
- Timestamp continuity must be preserved

## Solution Architecture

### Core Components

1. **TDC7200 State Management** (`tdc7200.cpp`)
2. **Configuration Change Tracking** (`TICC.ino`)
3. **Restart vs Resume Logic** (`TICC.ino`)
4. **User Choice Mechanism** (main loop)

### Key Data Structures

```cpp
// Configuration change tracking
static config_t config_backup;      // Backup of config before changes
static uint8_t config_changed = 0;  // Flag indicating config was modified
static uint8_t temp_config_mode = 0; // Flag for temporary config changes
```

## Implementation Details

### 1. TDC7200 Flush/Reset Functionality

**File**: `tdc7200.cpp`

#### `flush_and_reset()`
- Stops current TDC7200 measurements by clearing START_MEAS bit
- Acknowledges pending interrupts
- Waits for in-progress measurements to complete
- Re-enables measurement for next cycle
- Calls `reset_channel_state()` to clear software state

#### `reset_channel_state()`
- Clears measurement data (time1Result, time2Result, etc.)
- Resets timestamp data (tof, ts_split, etc.)
- Clears channel flags (new_ts_ready)
- Resets coarse-time cache
- **Preserves**: totalize counter, PICstop (for continuity)

### 2. Configuration Change Tracking

**File**: `config.cpp`

#### `MARK_CONFIG_CHANGED()` Macro
- Marks configuration as modified when parameters change
- Used throughout `processCommand()` function
- Tracks all configuration parameter modifications

#### Parameters Tracked
- Mode settings (A1-A6)
- Clock parameters (G1-G2)
- Channel settings (G3-G6)
- Basic settings (B, C, D, E, F)

### 3. Restart vs Resume Logic

**File**: `TICC.ino`

#### `config_change_requires_restart()`
Determines if hardware reinitialization is needed:

**Requires Restart** (hardware-affecting):
- `CLOCK_HZ` - Clock frequency changes
- `PICTICK_PS` - Coarse tick period changes  
- `CAL_PERIODS` - TDC calibration periods
- `START_EDGE` - Trigger edge configuration
- `SYNC_MODE` - Master/client sync mode

**Can Resume** (software-only):
- `MODE` - Measurement mode
- `POLL_CHAR` - Poll character
- `WRAP` - Timestamp wrap digits
- `NAME` - Channel names
- `PROP_DELAY` - Propagation delays
- `TIME_DILATION` - Time dilation factors
- `FIXED_TIME2` - Fixed time2 values
- `FUDGE0` - Fudge factors
- `TIMEOUT` - Measurement timeout

#### `apply_config_changes()`
Updates global variables and channel settings for resume:
- Updates global timing variables
- Updates channel-specific settings
- Preserves PICcount continuity

#### `flush_all_channels()`
Resets all TDC7200 channels:
- Calls `flush_and_reset()` on each channel
- Ensures clean state for resume

### 4. User Choice Mechanism

**File**: `TICC.ino` (main loop)

#### Configuration Menu Options
When user enters '#':
1. **T** - Temporary changes
   - Changes revert on restart
   - Not saved to EEPROM
   - Suitable for testing/experimentation

2. **P** - Permanent changes  
   - Changes saved to EEPROM
   - Persist across restarts
   - Production configuration

#### Process Flow
```
User enters '#' → Backup config → Show T/P options → User choice → 
Config menu → Track changes → Exit menu → 
Determine restart vs resume → Execute action
```

## Usage Examples

### Example 1: Temporary Mode Change
```
# (user enters '#')
Configuration Menu Options:
T - Temporary changes (revert on restart)
P - Permanent changes (save to EEPROM)
> T
# Temporary mode - changes will revert on restart
# (user changes mode from Timestamp to Interval)
# Changes are temporary and will revert on restart
# Applying configuration changes...
# Resuming operation with new settings.
```

### Example 2: Permanent Clock Change (Requires Restart)
```
# (user enters '#')
Configuration Menu Options:
T - Temporary changes (revert on restart)  
P - Permanent changes (save to EEPROM)
> P
# Permanent mode - changes will be saved to EEPROM
# (user changes clock frequency)
# Changes saved to EEPROM
# Configuration changes require restart. Restarting...
# (system restarts with new settings)
```

## State Management

### What's Preserved
- **PICcount**: Never reset, maintains timestamp continuity
- **totalize counters**: Channel event counts preserved
- **Coarse time reference**: Time base remains consistent

### What's Reset
- **TDC7200 measurements**: Partial measurements flushed
- **Channel state flags**: new_ts_ready, measurement data
- **Timestamp caches**: Coarse-time decomposition cache
- **Interrupt state**: TDC7200 interrupt flags cleared

### What's Updated
- **Global variables**: CLOCK_HZ, PICTICK_PS, etc.
- **Channel settings**: prop_delay, time_dilation, etc.
- **Configuration**: EEPROM (if permanent mode)

## Error Handling

### Restart Failures
- System falls back to safe defaults
- User notified of restart requirement
- Configuration changes may be lost

### Resume Failures  
- Channels are flushed to clean state
- Operation continues with new settings
- Partial measurements discarded

## Testing Considerations

### Test Scenarios
1. **Mode changes** (should resume)
2. **Clock changes** (should restart)
3. **Mid-measurement config** (should flush cleanly)
4. **Temporary vs permanent** (should behave differently)
5. **Multiple rapid changes** (should handle gracefully)

### Validation Points
- PICcount never resets during resume
- Timestamps remain continuous
- TDC7200 state is clean after flush
- Configuration changes are applied correctly

## Future Enhancements

### Potential Improvements
1. **Batch configuration**: Multiple changes before restart
2. **Validation**: Check parameter ranges before applying
3. **Rollback**: Undo changes if restart fails
4. **Status reporting**: Show current vs pending configuration
5. **Selective restart**: Restart only affected subsystems

### Configuration Categories
Could extend restart logic to be more granular:
- **Immediate**: Changes applied instantly
- **Channel flush**: Requires channel reset only
- **Hardware restart**: Requires full reinitialization
- **System restart**: Requires complete restart

## File Modifications

### Files Modified
- `TICC/tdc7200.h` - Added flush/reset method declarations
- `TICC/tdc7200.cpp` - Implemented flush/reset functionality
- `TICC/TICC.ino` - Added restart/resume logic and user interface
- `TICC/config.cpp` - Added configuration change tracking

### Key Functions Added
- `tdc7200Channel::flush_and_reset()`
- `tdc7200Channel::reset_channel_state()`
- `backup_config()`
- `config_change_requires_restart()`
- `apply_config_changes()`
- `flush_all_channels()`
- `handle_config_change_exit()`

## Version History

- **v1.0** (2025-01-09): Initial implementation
  - Basic restart/resume functionality
  - TDC7200 state management
  - User choice mechanism
  - Configuration change tracking

- **v1.1** (2025-01-09): Compilation fix
  - Fixed `config_changed` variable scope issue
  - Removed `static` keyword to make variable globally accessible
  - Resolved linking errors in config.cpp

- **v1.2** (2025-01-09): Simplified menu approach
  - Removed T/P choice at beginning
  - Added "W" command to write to EEPROM without restart
  - Updated exit options: discard, apply+restart, apply+resume, reset+restart
  - All changes are temporary unless written to EEPROM with "W" command

- **v1.3** (2025-01-09): Fixed double prompt issue
  - Fixed serial buffer handling for "#<enter>" input
  - Removed space+backspace rendering trick that caused double prompts
  - Modified readLine() to only echo newlines when there was actual input
  - Clean single "> " prompt now displayed
  - System tested and working properly with resume functionality

- **v1.4** (2025-01-09): Fixed measurement timing issue
  - Implemented flag-based config entry to complete current loop iteration
  - Added TDC7200 stop/start functionality to prevent new measurements during config
  - Added TDC7200 flush operation before config entry to clear pending measurements
  - Prevents timestamps from appearing after returning from config menu
  - Much cleaner implementation using config_requested flag
  - Ensures all pending measurements are processed and flushed before config entry
  - TDC7200 chips are stopped during config and restarted after config exit

---

*This documentation will be updated as the restart/resume functionality evolves.*
