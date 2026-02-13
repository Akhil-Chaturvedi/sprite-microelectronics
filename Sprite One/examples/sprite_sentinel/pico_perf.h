/*
 * pico_perf.h
 * Hardware abstraction for RP2040 performance & health monitoring
 * Implements "Self-Awareness" via internal ADC and Clock API
 */

#ifndef PICO_PERF_H
#define PICO_PERF_H

#ifdef ARDUINO_ARCH_RP2040
#include "hardware/adc.h"
#include "hardware/structs/vreg_and_chip_reset.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"

// Internal ADC Channel for Temperature is 4
#define ADC_TEMP_CHANNEL 4
#endif

class PicoPerf {
private:
  float _current_temp;
  uint32_t _current_freq_hz;
  
public:
  void begin() {
    #ifdef ARDUINO_ARCH_RP2040
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(ADC_TEMP_CHANNEL);
    _current_freq_hz = clock_get_hz(clk_sys);
    #else
    _current_freq_hz = 133000000;
    _current_temp = 25.0f;
    #endif
  }

  // Read internal temperature in Celsius
  float readTemperature() {
    #ifdef ARDUINO_ARCH_RP2040
    // 12-bit conversion, assume VREF = 3.3V
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;
    // Formula from RP2040 datasheet: T = 27 - (ADC_Voltage - 0.706)/0.001721
    _current_temp = 27.0f - (voltage - 0.706f) / 0.001721f;
    return _current_temp;
    #else
    // Simulation: Fluctuate slightly
    static float sim_temp = 35.0;
    sim_temp += (random(-10, 11) / 100.0);
    if (sim_temp > 80.0) sim_temp = 80.0;
    if (sim_temp < 25.0) sim_temp = 25.0;
    return sim_temp;
    #endif
  }

  // Set System Clock Frequency (DVFS)
  // freq_mhz: Target frequency in MHz (e.g. 50, 133, 250, 280)
  bool setPerformanceState(uint16_t freq_mhz) {
    #ifdef ARDUINO_ARCH_RP2040
    // Safety check: Don't melt the chip
    if (freq_mhz > 280) freq_mhz = 280;
    
    // Voltage scaling logic (Approximation)
    // 133MHz is Default (1.10V)
    // >133MHz needs higher voltage for stability
    vreg_voltage voltage = VREG_VOLTAGE_1_10;
    if (freq_mhz > 250) voltage = VREG_VOLTAGE_1_25; // Max voltage
    else if (freq_mhz > 200) voltage = VREG_VOLTAGE_1_20;
    else if (freq_mhz > 150) voltage = VREG_VOLTAGE_1_15;
    else if (freq_mhz < 100) voltage = VREG_VOLTAGE_0_95; // Undervolt for power saving
    
    vreg_set_voltage(voltage);
    delay(1); // Settling time
    
    // Set system clock
    if (set_sys_clock_khz(freq_mhz * 1000, true)) {
      _current_freq_hz = freq_mhz * 1000000;
      
      // Re-configure serial because baud rate depends on sys clock
      Serial.end();
      Serial1.end();
      Serial.begin(115200);
      Serial1.begin(115200);
      return true;
    }
    return false;
    #else
    _current_freq_hz = freq_mhz * 1000000;
    return true;
    #endif
  }

  uint32_t getFrequency() {
    return _current_freq_hz;
  }
};

static PicoPerf perf;

#endif // PICO_PERF_H
