/*
 * reflex_arc.h
 * PIO Program for "Spinal Cord" Reflexes
 * Monitors a pin for rapid changes and triggers IRQ without CPU intervention.
 */

#ifndef REFLEX_ARC_H
#define REFLEX_ARC_H

#ifdef ARDUINO_ARCH_RP2040
#include "hardware/pio.h"
#endif

// Mock PIO program data
static const uint16_t reflex_program_instructions[] = {
    // .wrap_target
    0xe000, //  0: set    pins, 0
    0x2020, //  1: wait   0 pin, 0
    0x20a0, //  2: wait   1 pin, 0
    0xc000, //  3: irq    nowait 0
    // .wrap
};

static const struct pio_program reflex_program = {
    .instructions = reflex_program_instructions,
    .length = 4,
    .origin = -1,
};

class ReflexArc {
public:
    static void init(uint8_t pin) {
        #ifdef ARDUINO_ARCH_RP2040
        PIO pio = pio0;
        uint offset = pio_add_program(pio, &reflex_program);
        uint sm = pio_claim_unused_sm(pio, true);
        
        pio_sm_config c = pio_get_default_sm_config();
        sm_config_set_in_pins(&c, pin);
        sm_config_set_jmp_pin(&c, pin);
        
        pio_sm_init(pio, sm, offset, &c);
        pio_sm_set_enabled(pio, sm, true);
        #endif
    }
};

#endif // REFLEX_ARC_H
