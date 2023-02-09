#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "cc_interface.h"
#include "pio_serialiser.pio.h"



// these pin numbers are the GP.. numbers, not the numbers on the PCB

#define GLITCH_PIN 15
// set this to pin 14 to check with logic analyser the time offset of the glitch
// #define GLITCH_PIN 14
#define CCDEBUG_CLK 2
#define CCDEBUG_DATA 3
#define CCDEBUG_RESET 4
#define POWER_PIN 16

// send a bit every PIO_SERIAL_CLKDIV clock cycles
#define PIO_SERIAL_CLKDIV 1.f

// size of glitch buffer, every entry accounts for 32 samples
#define GLITCH_BUFFER_SIZE (6117)

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int dma_chan;

// last entry will always be zero, to ensure the glitching mosfet is pulled low again
static uint32_t wavetable[GLITCH_BUFFER_SIZE + 1] = {0};

void initial_setup_dma() {
    uint offset = pio_add_program(pio0, &pio_serialiser_program);
    pio_serialiser_program_init(pio0, 0, offset, GLITCH_PIN, PIO_SERIAL_CLKDIV);

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0);

    dma_channel_configure(
        dma_chan,
        &c,
        &pio0_hw->txf[0],   // Write address (only need to set this once)
        NULL,               // Don't provide a read address yet
        GLITCH_BUFFER_SIZE+1, // Write the same value many times, then halt and interrupt
        false               // Don't start yet
    );
}

void prepare_dma() {
    dma_channel_set_read_addr(dma_chan, wavetable, false);
}

uint8_t opcodes[4] = {
    0x90, // MOV DPTR,#data16   : Load data pointer with a 16-bit constant
    0x00, // address high
    0x00, // address low
    0xE0, // MOVX A,@DPTR       : Move external RAM (16-bit address) to A
};

void core1_entry() { // the main() function on the second core
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(POWER_PIN);
    gpio_set_dir(POWER_PIN, GPIO_OUT);
    gpio_set_drive_strength(POWER_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(POWER_PIN, GPIO_SLEW_RATE_FAST);
    initial_setup_dma();
    gpio_put(POWER_PIN, 1);
    cc.begin(CCDEBUG_CLK, CCDEBUG_DATA, CCDEBUG_RESET);
    while (true) {
        int c = getchar_timeout_us(0);
        if (c < 0) {
            continue;
        }
        switch (c) {
        case 'b': { // put in debug mode and get serial number
            gpio_put(POWER_PIN, 0);
            busy_wait_ms(50);
            gpio_put(POWER_PIN, 1);
            busy_wait_ms(50);
            cc.reset_cc();
            cc.enable_cc_debug();
            cc.opcode(0x00); // execute NOP to update status byte of debugger
            uint16_t serial = cc.get_serial_number();
            // stop timing sensitive code
            putchar(serial >> 8);
            putchar(serial & 0xff);
            putchar(cc.send_cc_cmdS(0x34));
            putchar('.');
            break;
        }
        case 's': { // get size of glitch buffer
            printf("%d.", GLITCH_BUFFER_SIZE);
            break;
        }
        case 'd': { // set glitch data
            for (uint i = 0; i < GLITCH_BUFFER_SIZE; i++) {
                uint32_t entry = 0;
                for (uint j = 0; j < 4; j++) {
                    int read = getchar_timeout_us(0);
                    while (read < 0) {read = getchar_timeout_us(0);}
                    entry = (entry << 8) | ((uint8_t) read);
                }
                wavetable[i] = entry;
            }
            printf("d.");
            break;
        }

        case 'a': { // set opcodes for glitching
            for (int i = 0; i < 4; i++) {
                int read = getchar_timeout_us(0);
                while (read < 0) {read = getchar_timeout_us(0);}
                opcodes[i] = (uint8_t) (read & 0xff);
            }
            printf("a.");
            break;
        }

        case 'g': { // glitch! (at 130 and 690 us)
            prepare_dma();
            uint32_t saved_interrupt_config = save_and_disable_interrupts();
            gpio_put(POWER_PIN, 1);
            busy_wait_us(20000); // 20000 works, less sometimes isn't long enough for the chip to boot
            // ^ this is a good place to optimize for time
            
            // START TIMING SENSITIVE SECTION
            dma_channel_start(dma_chan);
            cc.reset_cc();
            cc.enable_cc_debug();

            // 1 = DEBUG_INSTR <- at end of this is the glitch vulnerability
            // 2 = first byte in opcode
            // 3 = second byte in opcode
            // 4 = third byte in opcode
            // 5 = answer
            // 6 = READ_STATUS
            // 7 = answer
            // 8 = DEBUG_INSTR <- at end of this is the glitch vulnerability
            // 9 = first byte in opcode
            // 10 = answer
            // 11 = READ_STATUS
            // 12 = answer
            uint8_t first_result = cc.opcode(opcodes[0], opcodes[1], opcodes[2]);
            uint8_t first_status = cc.send_cc_cmdS(0x34);
            cc.cc_send_byte(0x65); // this has a further edit distance to the erase command than the single opcode command
            cc.cc_send_byte(opcodes[3]);
            uint8_t second_result = cc.cc_receive_byte();
            uint8_t second_status = cc.send_cc_cmdS(0x34);

            // END TIMING SENSITIVE SECTION

            gpio_put(POWER_PIN, 0);
            busy_wait_ms(5);
            restore_interrupts(saved_interrupt_config);
            putchar(first_result);
            putchar(first_status);
            putchar(second_result);
            putchar(second_status);
            printf(".");
            break;
        }

        case 'p': { // wipe chip
            // protect against accidentally wiping
            uint64_t password = 0;
            for (int i = 0; i < 8; i++) {
                int read = getchar_timeout_us(0);
                while (read < 0) {read = getchar_timeout_us(0);}
                password = (password << 8) | (read&0xff);
            }
            if (password != 0xAABBCCDDEEFF0102) {
                printf("no.");
            } else {
                uint64_t number = 0;
                for (int i = 0; i < 8; i++) {
                    int read = getchar_timeout_us(0);
                    while (read < 0) {read = getchar_timeout_us(0);}
                    number = (number << 8) | (read&0xff);
                }
                
                gpio_put(POWER_PIN, 1);
                busy_wait_ms(500);
                cc.reset_cc();
                cc.enable_cc_debug();
                cc.send_cc_cmdS(0x14);
                busy_wait_us(number);
                gpio_put(POWER_PIN, 0);
                busy_wait_ms(100);
                gpio_put(POWER_PIN, 1);
                busy_wait_ms(500);
                cc.reset_cc();
                cc.enable_cc_debug();
                putchar('p');
                putchar(cc.send_cc_cmdS(0x34));
                putchar('.');
            }    
            break;
        }

        case 'w': {  // write to chip
            gpio_put(POWER_PIN, 0);
            busy_wait_ms(50);
            gpio_put(POWER_PIN, 1);
            busy_wait_ms(50);
            cc.reset_cc();
            cc.enable_cc_debug();
            uint8_t buffer[0x8000] = {0x23};
            for (int i = 0; i < 0x8000; i++) {
                buffer[i] = (i & 0xff);
            }
            buffer[0] = 0x02; // ljump to 0000
            buffer[1] = 0;
            buffer[2] = 0;

            cc.write_code_memory(0, buffer, 0x8000);
            printf("w.");
            break;
        }

        case 'r': { // read out chip
            gpio_put(POWER_PIN, 1);
            busy_wait_ms(50);

            uint8_t buffer[0x8000] = {0x42};

            cc.reset_cc();
            cc.enable_cc_debug();
            cc.read_xdata_memory(0x0, 0x8000, buffer);
            for (int i = 0; i < 0x8000; i++) {
                putchar(buffer[i]);
            }
            break;
        }

        case 'l': { // set lock bits to lock out debug interface
            gpio_put(POWER_PIN, 0);
            busy_wait_ms(50);
            gpio_put(POWER_PIN, 1);
            busy_wait_ms(50);
            cc.reset_cc();
            cc.enable_cc_debug();
            cc.set_lock_byte(0);
            busy_wait_ms(100);
            cc.reset_cc();
            cc.enable_cc_debug();
            cc.opcode(0x00); // execute NOP to update status byte
            putchar(cc.send_cc_cmdS(0x34));
            break;
        }

        case '\n':
        case '\r':
            break;
        default:
            printf("?%c.", c);
        }
    }
}

int main() {
    set_sys_clock_khz(250000, true);
    stdio_init_all();
    multicore_launch_core1(core1_entry);
    while (true) {
        sleep_ms(1);
    }
}
