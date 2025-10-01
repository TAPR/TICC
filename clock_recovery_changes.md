# Clock Recovery Changes Documentation

## Changes to TICC.ino

### New enum for clock states
```cpp
enum clock_state_t {
  CLOCK_OK,
  CLOCK_LOST_MANUAL,
  CLOCK_LOST_AUTO_WAIT,
  CLOCK_LOST_AUTO_RESTART
};
```

### New static variables in loop()
```cpp
static clock_state_t clock_state = CLOCK_OK;
static unsigned long clock_lost_time = 0;
static unsigned long last_clock_check_time = 0;
static uint8_t cached_auto_restart_mode = 0;
static uint16_t cached_restart_timeout_ms = 0;
```

### Modified loop() function structure
1. Check for '#' config menu entry (moved to top priority)
2. Check clock state and handle accordingly
3. Normal clock detection logic

### Clock state handling logic
- CLOCK_OK: Normal operation
- CLOCK_LOST_MANUAL: Wait for user key press, then restart when clock restored
- CLOCK_LOST_AUTO_WAIT: Wait for clock restoration, then start timeout timer
- CLOCK_LOST_AUTO_RESTART: Restart immediately when clock restored

### Key functions added
- `update_cached_clock_restart_config()` - Updates cached values from config
- Clock restoration detection and state transitions
- User input handling during clock loss states

## Changes to config_t struct

### New fields to add at END of struct
```cpp
// clock restoration settings (added in version 13)
uint8_t    AUTO_RESTART_MODE;         // clock restoration mode (0=manual, 1=auto with timeout, 2=immediate auto)
uint16_t   RESTART_TIMEOUT_SEC;       // timeout for auto restart mode (seconds)
```

### New constants in config.h
```cpp
#define DEFAULT_AUTO_RESTART_MODE 0
#define DEFAULT_RESTART_TIMEOUT_SEC 30
#define EEPROM_VERSION 13  // Increment from 12 to 13
```

### Changes to config_core.cpp
- Update `defaultConfig()` to initialize new fields at the end
- Update `print_config()` to display clock restoration settings
- Call `update_cached_clock_restart_config()` in `apply_config_changes()`

### Changes to config_menu.cpp
- Add K submenu for clock restoration settings
- Handle K1, K2, K3 commands for different restart modes
- Display current clock restoration behavior

## Memory Impact
The clock recovery functionality adds approximately 1135 bytes (14%) to global memory usage due to:
- Additional static variables in loop()
- New config struct fields
- Additional menu strings and logic
- Cached values for performance optimization

## Key Design Principles
1. Clock loss detection uses minimal cycles during normal operation
2. System halts measurement processing when clock is lost
3. Three restart modes: manual, auto with timeout, immediate auto
4. Timeout only starts after clock is restored
5. User can force immediate restart with 'r' key in auto modes
6. Config changes are cached for performance

