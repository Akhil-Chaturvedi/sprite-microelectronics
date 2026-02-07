// ==============================================================================
// Sprite One - PIO SPI Interface
// ==============================================================================
// C++ wrapper for PIO SPI slave state machine
// Handles initialization, interrupts, and data transfer
// ==============================================================================

#ifndef PIO_SPI_H
#define PIO_SPI_H

#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

// Pin definitions for SPI interface
#define SPI_MOSI_PIN  3  // Host → Sprite (data in)
#define SPI_MISO_PIN  4  // Sprite → Host (data out)
#define SPI_SCK_PIN   2  // Clock from host
#define SPI_CS_PIN    5  // Chip select (active low)

// PIO instance and state machine
#define SPI_PIO       pio0
#define SPI_SM        0

// Circular buffer for received bytes
#define RX_BUFFER_SIZE 256
struct RxBuffer {
    uint8_t data[RX_BUFFER_SIZE];
    volatile uint16_t write_pos;
    volatile uint16_t read_pos;
};

class PioSpi {
public:
    PioSpi();
    
    // Initialize PIO SPI slave
    bool begin();
    
    // Check if data is available
    bool available();
    
    // Read single byte (blocking if empty)
    uint8_t read();
    
    // Read multiple bytes
    uint16_t read(uint8_t* buffer, uint16_t length);
    
    // Write single byte to TX FIFO
    void write(uint8_t data);
    
    // Write multiple bytes
    void write(const uint8_t* buffer, uint16_t length);
    
    // Get number of bytes available
    uint16_t bytesAvailable();
    
    // Clear receive buffer
    void flush();
    
    // Statistics
    uint32_t getRxCount() { return rx_count; }
    uint32_t getTxCount() { return tx_count; }
    uint32_t getErrorCount() { return error_count; }

private:
    RxBuffer rx_buffer;
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;
    
    // PIO interrupt handler (called when byte received)
    static void irqHandler();
    static PioSpi* instance; // For ISR access
    
    // Helper functions
    void bufferPush(uint8_t byte);
    bool bufferPop(uint8_t* byte);
};

#endif // PIO_SPI_H
