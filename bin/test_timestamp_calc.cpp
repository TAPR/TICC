/*
 * test_timestamp_calc.cpp - Test suite for TICC timestamp calculation
 * 
 * Compile: g++ -std=c++11 -o test_timestamp_calc test_timestamp_calc.cpp
 * Run: ./test_timestamp_calc
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Constants from config.h
#define PS_PER_SEC ((int64_t)1000000000000)  // ps/s
#define DEFAULT_PICTICK_PS ((int64_t)100000000)  // 100us

// Timestamp structure from tdc7200.h
struct Timestamp64 {
  int32_t seconds;
  uint64_t picos;
};

// Simulate Arduino Serial for testing
class FakeSerial {
public:
  void print(const char* s) { printf("%s", s); }
  void print(int n) { printf("%d", n); }
  void print(long n) { printf("%ld", n); }
  void println() { printf("\n"); }
  void println(const char* s) { printf("%s\n", s); }
  void write(const uint8_t* buf, size_t len) { /* no-op for tests */ }
  int availableForWrite() { return 1000; }
};

FakeSerial Serial;

// Channel structure (minimal for testing)
struct TestChannel {
  int64_t PICstop;
  int64_t last_picstop;
  int32_t tof;
  int32_t last_tof;
  Timestamp64 timestamp;
  Timestamp64 last_timestamp;
  uint8_t new_ts_ready;
  int64_t totalize;
  char name;
};

// CRC function from timestamps.cpp
uint8_t crc8_maxim(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  while (len--) {
    uint8_t in = *data++;
    crc ^= in;
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x01) {
        crc = (crc >> 1) ^ 0x8C;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

// Core timestamp calculation from timestamps.cpp
void calculate_timestamp_core(TestChannel* channel, int64_t pictick_ps) {
  if (!channel) return;
  
  // Preserve last values
  channel->last_tof = channel->tof;
  channel->last_timestamp = channel->timestamp;
  
  // Note: In real code, we'd read TDC and call ready_next() here
  // For testing, tof and PICstop are pre-set
  
  // Calculate delta ticks since previous event on this channel
  int64_t picstop_now64, last_picstop64;
  // Note: No noInterrupts() needed in host test
  picstop_now64 = channel->PICstop;
  last_picstop64 = channel->last_picstop;
  channel->last_picstop = picstop_now64;
  int64_t dcount = picstop_now64 - last_picstop64;

  // Calculate delta in picoseconds
  int64_t delta_ps = dcount * pictick_ps
                   - (int64_t)channel->tof
                   + (int64_t)channel->last_tof;

  // Mixed-radix accumulation with automatic carry/borrow
  if (delta_ps >= 0) {
    channel->timestamp.picos += (uint64_t)delta_ps;
    // Handle multiple second carries when delta_ps is large (e.g., after input gaps)
    while (channel->timestamp.picos >= PS_PER_SEC) {
      channel->timestamp.picos -= PS_PER_SEC;
      channel->timestamp.seconds += 1;
    }
  } else {
    uint64_t m = (uint64_t)(-delta_ps);
    if (m <= channel->timestamp.picos) {
      channel->timestamp.picos -= m;
    } else {
      m -= channel->timestamp.picos;
      uint64_t borrow_sec = 1 + (m / PS_PER_SEC);
      uint64_t rem = m % PS_PER_SEC;
      channel->timestamp.seconds -= (int32_t)borrow_sec;
      channel->timestamp.picos = PS_PER_SEC - rem;
    }
  }

  // Mark timestamp as ready and increment counter
  channel->new_ts_ready = 1;
  channel->totalize++;
}

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

void test_report(const char* test_name, bool passed) {
  if (passed) {
    printf("  ✓ PASS: %s\n", test_name);
    tests_passed++;
  } else {
    printf("  ✗ FAIL: %s\n", test_name);
    tests_failed++;
  }
}

// Helper to check timestamp equality
bool timestamp_equal(const Timestamp64* a, int32_t exp_sec, uint64_t exp_picos) {
  return (a->seconds == exp_sec) && (a->picos == exp_picos);
}

// Test 1: Normal consecutive 1 PPS events
void test_normal_1pps() {
  printf("\nTest 1: Normal consecutive 1 PPS events\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Event 1: PICstop=10000 (1 second), tof=50000000 (50ms)
  ch.PICstop = 10000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Check: Should be ~0.95 seconds (1.0s - 0.05s)
  test_report("Event 1 timestamp ~0.95s", 
    ch.timestamp.seconds == 0 && ch.timestamp.picos > 900000000000LL);
  
  // Event 2: PICstop=20000 (2 seconds), tof=50000000 (50ms)
  ch.PICstop = 20000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Check: Should be ~1.95 seconds (delta = 1 second)
  test_report("Event 2 timestamp ~1.95s",
    ch.timestamp.seconds == 1 && ch.timestamp.picos > 900000000000LL);
}

// Test 2: Large gap between events (simulating input interruption)
void test_large_gap() {
  printf("\nTest 2: Large gap between events (17 second gap)\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Event 1: PICstop=10000, tof=50000000
  ch.PICstop = 10000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  int32_t ts1_sec = ch.timestamp.seconds;
  
  // Event 2: PICstop=180000 (17 second gap), tof=50000000
  ch.PICstop = 180000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Check: Should be ~17 seconds more than event 1
  int32_t delta_sec = ch.timestamp.seconds - ts1_sec;
  test_report("17 second gap reflected correctly", delta_sec == 17);
}

// Test 3: Very large gap (hours)
void test_very_large_gap() {
  printf("\nTest 3: Very large gap (1 hour = 3600 seconds)\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Event 1: PICstop=10000, tof=50000000
  ch.PICstop = 10000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  int32_t ts1_sec = ch.timestamp.seconds;
  
  // Event 2: PICstop=36010000 (3600 second gap), tof=50000000
  ch.PICstop = 36010000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Check: Should be 3600 seconds more
  int32_t delta_sec = ch.timestamp.seconds - ts1_sec;
  test_report("3600 second gap reflected correctly", delta_sec == 3600);
}

// Test 4: Negative delta (timestamp going backwards - error condition)
void test_negative_delta() {
  printf("\nTest 4: Negative delta handling\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Event 1: PICstop=10000, tof=50000000
  ch.PICstop = 10000;
  ch.tof = 50000000;
  ch.timestamp.seconds = 10;
  ch.timestamp.picos = 500000000000LL;  // 10.5 seconds
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Event 2: PICstop=9000 (backwards!), tof=50000000
  ch.PICstop = 9000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Should handle negative delta without crashing
  test_report("Negative delta doesn't crash", ch.timestamp.seconds >= 0);
}

// Test 5: Varying tof values (edge cases)
void test_varying_tof() {
  printf("\nTest 5: Varying tof values (min and max range)\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Event 1: Minimum tof (300,000 ps = 0.3 ms)
  ch.PICstop = 10000;
  ch.tof = 300000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  test_report("Min tof (300,000 ps) works", ch.new_ts_ready == 1);
  
  // Event 2: Maximum tof (100,300,000 ps = 100.3 ms)
  ch.PICstop = 20000;
  ch.tof = 100300000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  test_report("Max tof (100,300,000 ps) works", ch.timestamp.seconds >= 0);
}

// Test 6: Picosecond overflow boundary
void test_pico_overflow() {
  printf("\nTest 6: Picosecond field near overflow boundary\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Start with timestamp near 1 second boundary
  ch.timestamp.seconds = 5;
  ch.timestamp.picos = 999900000000LL;  // 0.9999 seconds
  ch.PICstop = 10000;
  ch.tof = 50000000;
  ch.last_picstop = 10000;
  ch.last_tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Add small delta that should push it over 1 second
  ch.PICstop = 10001;  // 0.1 ms = 100,000 ps
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Should have carried to next second
  test_report("Pico overflow carries correctly", ch.timestamp.seconds == 6);
}

// Test 7: CRC-8 correctness
void test_crc8() {
  printf("\nTest 7: CRC-8 calculation\n");
  
  uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
  uint8_t crc = crc8_maxim(test_data, 4);
  
  test_report("CRC-8 returns valid value", crc != 0);
  
  // Test that same data produces same CRC
  uint8_t crc2 = crc8_maxim(test_data, 4);
  test_report("CRC-8 is deterministic", crc == crc2);
}

// Test 8: Sequential events with small gaps
void test_sequential_small_gaps() {
  printf("\nTest 8: Sequential events with varying small gaps\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  int64_t picstop = 1000;
  int32_t tof = 50000000;
  
  // Simulate 100 events with 1 second gaps
  for (int i = 0; i < 100; i++) {
    ch.PICstop = picstop;
    ch.tof = tof;
    calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
    picstop += 10000;  // 1 second gap
    tof += (i % 2) * 1000;  // Slight tof variation
  }
  
  // After 100 events with 1 second gaps, should be ~99-100 seconds
  test_report("100 sequential events reach ~99-100 seconds",
    ch.timestamp.seconds >= 98 && ch.timestamp.seconds <= 101);
}

// Test 9: Multiple large gaps
void test_multiple_large_gaps() {
  printf("\nTest 9: Multiple large gaps\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Event 1
  ch.PICstop = 10000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Gap 1: 60 seconds
  ch.PICstop = 610000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  test_report("First 60 second gap", ch.timestamp.seconds >= 59 && ch.timestamp.seconds <= 61);
  
  // Gap 2: 120 seconds
  ch.PICstop = 1810000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  test_report("Second 120 second gap", ch.timestamp.seconds >= 178 && ch.timestamp.seconds <= 182);
}

// Test 10: Edge case - zero gap (duplicate event detection scenario)
void test_zero_gap() {
  printf("\nTest 10: Zero gap between events\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Event 1
  ch.PICstop = 10000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  Timestamp64 ts1 = ch.timestamp;
  
  // Event 2: Same PICstop (zero gap) but different tof
  ch.PICstop = 10000;
  ch.tof = 60000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Timestamp should only change by tof difference
  int64_t delta = (int64_t)ch.timestamp.picos - (int64_t)ts1.picos;
  test_report("Zero gap handled correctly", delta < 100000000);  // Less than 100ms change
}

// Test 11: Small integer parts with varying fractional parts
void test_small_integers() {
  printf("\nTest 11: Small integer parts (1-10 seconds) with varying fractional parts\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  uint64_t test_picos[] = {
    0LL,                      // 0.0 seconds
    1000000LL,                // 1 microsecond
    1000000000LL,             // 1 millisecond
    100000000000LL,           // 0.1 seconds
    500000000000LL,           // 0.5 seconds
    999999999999LL,           // 0.999999999999 seconds (near boundary)
    123456789012LL,           // Random fractional part
    987654321098LL            // Random fractional part
  };
  
  int passed = 0;
  for (size_t i = 0; i < sizeof(test_picos)/sizeof(test_picos[0]); i++) {
    ch.timestamp.seconds = 0;
    ch.timestamp.picos = test_picos[i];
    ch.PICstop = 10000 * (i + 1);
    ch.tof = 50000000;
    ch.last_picstop = 10000 * i;
    ch.last_tof = 50000000;
    
    calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
    
    // Should advance by approximately 1 second
    if (ch.timestamp.seconds >= 0 && ch.timestamp.seconds <= 2) {
      passed++;
    }
  }
  
  test_report("All fractional parts handled correctly", passed == 8);
}

// Test 12: Large integer parts (100K+ seconds)
void test_large_integers() {
  printf("\nTest 12: Large integer parts (100K+ seconds)\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Start at 100,000 seconds
  ch.timestamp.seconds = 100000;
  ch.timestamp.picos = 123456789012LL;
  ch.PICstop = 1000000000LL;
  ch.tof = 50000000;
  ch.last_picstop = 1000000000LL;
  ch.last_tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  // Add 1 hour gap
  ch.PICstop = 1036000000LL;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  test_report("Large integer (100K+) with hour gap",
    ch.timestamp.seconds >= 103599 && ch.timestamp.seconds <= 103601);
}

// Test 13: Microsecond boundary testing
void test_microsecond_boundaries() {
  printf("\nTest 13: Microsecond boundary precision (1 µs steps)\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  // Test with 1 µs increments in fractional part
  uint64_t picos_1us = 1000000LL;  // 1 microsecond
  
  ch.timestamp.seconds = 0;
  ch.timestamp.picos = 0;
  
  int passed = 0;
  for (int i = 0; i < 10; i++) {
    ch.PICstop = 10000 + i;  // Small increment
    ch.tof = 50000000 - (i * 1000000);  // Vary by 1 µs
    
    calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
    
    // Verify timestamp is reasonable
    if (ch.timestamp.seconds >= 0 && ch.timestamp.picos < PS_PER_SEC) {
      passed++;
    }
  }
  
  test_report("Microsecond boundaries handled", passed == 10);
}

// Test 14: Random stress test with 1000 events
void test_random_stress() {
  printf("\nTest 14: Random stress test (1000 events)\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  srand(42);  // Fixed seed for reproducibility
  
  int64_t picstop = 1000;
  int passed = 0;
  
  for (int i = 0; i < 1000; i++) {
    // Random gap: 1 to 100 ticks (0.1ms to 10ms)
    int64_t gap = 1 + (rand() % 100);
    picstop += gap;
    
    ch.PICstop = picstop;
    // Random tof in valid range: 300,000 to 100,300,000 ps
    ch.tof = 300000 + (rand() % 100000000);
    
    calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
    
    // Verify timestamp is valid (no overflow, reasonable value)
    if (ch.timestamp.picos < PS_PER_SEC && ch.timestamp.seconds >= 0) {
      passed++;
    }
  }
  
  test_report("1000 random events processed correctly", passed == 1000);
  printf("    Final timestamp: %ld.%012llu\n", 
         (long)ch.timestamp.seconds, 
         (unsigned long long)ch.timestamp.picos);
}

// Test 15: Extreme picos values near boundaries
void test_extreme_picos() {
  printf("\nTest 15: Extreme picosecond values near boundaries\n");
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  uint64_t extreme_picos[] = {
    0LL,                          // Minimum
    1LL,                          // Near minimum
    999999999998LL,               // Near maximum
    999999999999LL,               // Maximum (1 ps before 1 second)
    PS_PER_SEC - 1000000LL,       // 1 µs before boundary
    PS_PER_SEC - 1LL              // 1 ps before boundary
  };
  
  int passed = 0;
  for (size_t i = 0; i < sizeof(extreme_picos)/sizeof(extreme_picos[0]); i++) {
    ch.timestamp.seconds = 5;
    ch.timestamp.picos = extreme_picos[i];
    ch.PICstop = 10000 * (i + 1);
    ch.tof = 50000000;
    ch.last_picstop = 10000 * i;
    ch.last_tof = 50000000;
    
    calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
    
    // Verify picos stayed within bounds and seconds incremented correctly
    if (ch.timestamp.picos < PS_PER_SEC && ch.timestamp.seconds >= 5) {
      passed++;
    }
  }
  
  test_report("Extreme pico values handled correctly", passed == 6);
}

// Test 16: Large gap with random fractional parts
void test_large_gap_random_fractions() {
  printf("\nTest 16: Large gaps (10-1000 seconds) with random fractional parts\n");
  
  srand(123);  // Fixed seed
  
  TestChannel ch = {0};
  ch.name = 'A';
  
  ch.PICstop = 10000;
  ch.tof = 50000000;
  calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
  
  int passed = 0;
  for (int i = 0; i < 20; i++) {
    int32_t prev_sec = ch.timestamp.seconds;
    
    // Random gap: 10 to 1000 seconds
    int64_t gap_seconds = 10 + (rand() % 990);
    ch.PICstop += gap_seconds * 10000;  // Convert to ticks
    
    // Random tof
    ch.tof = 300000 + (rand() % 100000000);
    
    calculate_timestamp_core(&ch, DEFAULT_PICTICK_PS);
    
    // Verify timestamp advanced by approximately the gap
    int32_t actual_delta = ch.timestamp.seconds - prev_sec;
    if (actual_delta >= gap_seconds - 1 && actual_delta <= gap_seconds + 1) {
      passed++;
    }
  }
  
  test_report("20 random large gaps handled correctly", passed == 20);
}

int main() {
  printf("=================================================\n");
  printf("TICC Timestamp Calculation Test Suite\n");
  printf("Date: October 7, 2025\n");
  printf("=================================================\n");
  
  test_normal_1pps();
  test_large_gap();
  test_very_large_gap();
  test_negative_delta();
  test_varying_tof();
  test_pico_overflow();
  test_crc8();
  test_sequential_small_gaps();
  test_multiple_large_gaps();
  test_zero_gap();
  test_small_integers();
  test_large_integers();
  test_microsecond_boundaries();
  test_random_stress();
  test_extreme_picos();
  test_large_gap_random_fractions();
  
  printf("\n=================================================\n");
  printf("Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
  printf("=================================================\n");
  
  return (tests_failed == 0) ? 0 : 1;
}

