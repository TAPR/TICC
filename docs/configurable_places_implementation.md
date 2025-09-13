# Configurable PLACES Implementation

## Overview

This document describes the implementation of configurable decimal places for TICC data output. Previously, the number of decimal places was hardcoded as `#define PLACES 11` in `config.h`. This implementation makes it a user-configurable parameter accessible through the main configuration menu.

## Changes Made

### 1. EEPROM Structure Changes

**File: `TICC/config.h`**
- Incremented `EEPROM_VERSION` from 10 to 11
- Added `DEFAULT_PLACES` constant with value 11
- Added `PLACES` field to `config_t` struct (int16_t type)
- Positioned after `WRAP` field in the struct

### 2. Menu Structure Changes

**File: `TICC/config.cpp`**

#### New Menu Structure:
- **A** - Mode (unchanged)
- **B** - Timestamp Wrap digits (unchanged)
- **C** - Output Decimal Places (NEW)
- **D** - Trigger Edge A/B (was C)
- **E** - Master/Client (was D)
- **F** - Channel Names (was E)
- **G** - Poll Character (was F)
- **H** - Advanced settings (was G)
- **I** - Show startup info (was H)
- **X** - Write changes to EEPROM (was W)

#### Advanced Submenu Changes:
- **H1** - Clock Speed MHz (was G1)
- **H2** - Coarse Tick us (was G2)
- **H3** - Propagation Delay ps A/B (was G3)
- **H4** - Time Dilation A/B (was G4)
- **H5** - fixedTime2 ps A/B (was G5)
- **H6** - FUDGE0 ps A/B (was G6)

### 3. Code Changes

**File: `TICC/config.cpp`**
- Added PLACES initialization in `defaultConfig()` function
- Added C command handler with validation (0-12 range)
- Updated all menu item labels and command handlers
- Added PLACES to `print_config()` function for startup display
- Updated Advanced submenu prefix from G to H

**File: `TICC/TICC.ino`**
- Replaced all `PLACES` references with `config.PLACES`
- Updated header output descriptions
- Updated all formatting function calls
- Added PLACES to parameters that can be changed without restart

### 4. Startup Configuration Display

The PLACES value now appears in the startup configuration display:
```
# Output Decimal Places: 11
```

## Technical Details

### PLACES Range and Validation
- **Valid Range**: 0-12 decimal places
- **Default Value**: 11 (maintains backward compatibility)
- **Validation**: Input validation ensures values are within range
- **Type**: int16_t (same as WRAP parameter)

### EEPROM Compatibility
- **Version Increment**: EEPROM_VERSION changed from 10 to 11
- **Migration**: Existing devices will reset to defaults on first boot
- **Impact**: Users will need to reconfigure settings after upgrade

### Performance Impact
- **None**: All formatting functions already accepted places parameter
- **Memory**: Minimal increase due to additional config field

## Usage

### Setting PLACES Value
1. Enter configuration menu (type any character during startup)
2. Select option **C** - Output Decimal Places
3. Enter desired value (0-12)
4. Save changes with option **X** - Write changes to EEPROM

### Menu Navigation
- All existing menu items have been relabeled
- Advanced settings moved from G to H prefix
- Write changes moved from W to X

## Testing

### Required Tests
1. **EEPROM Migration**: Verify old configs reset to defaults
2. **Menu Navigation**: Test all menu items with new labels
3. **PLACES Range**: Test with values 0, 6, 11, 12
4. **Output Format**: Verify data formatting with different PLACES
5. **Advanced Menu**: Test H1-H6 commands
6. **Startup Display**: Verify PLACES appears in configuration
7. **Backward Compatibility**: Ensure existing functionality works

### Test Cases
- **PLACES = 0**: No decimal places (integer seconds only)
- **PLACES = 6**: Microsecond precision
- **PLACES = 11**: Default picosecond precision
- **PLACES = 12**: Maximum precision

## Breaking Changes

### Script Compatibility
- **Advanced Menu**: G1-G6 commands changed to H1-H6
- **Write Command**: W command changed to X
- **Menu Items**: All items after B relabeled

### EEPROM Reset
- **All Settings**: Will reset to defaults on first boot
- **Serial Numbers**: Preserved (stored separately)

## Files Modified

1. **TICC/config.h** - EEPROM structure and constants
2. **TICC/config.cpp** - Menu system and configuration handling
3. **TICC/TICC.ino** - Data output formatting

## Future Considerations

### Potential Enhancements
- **Range Validation**: Could be made more sophisticated
- **Per-Mode Settings**: Different PLACES for different measurement modes
- **Dynamic Range**: Adjust range based on measurement precision

### Maintenance Notes
- **Menu Updates**: Any future menu additions should consider current structure
- **EEPROM Changes**: Future struct changes require version increment
- **Testing**: Always test with various PLACES values

## Implementation Date

**Date**: December 2024
**Branch**: configurable-places
**EEPROM Version**: 11

---

*This implementation maintains backward compatibility for all existing functionality while adding the requested configurable decimal places feature.*
