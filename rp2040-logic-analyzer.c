/**
 * Modified by Mark Komus 2021
 * Now repeatedly captures data and outputs to a CSV format
 * Intended to be imported by sigrok / PulseView
 *
 */

/**
 *
 * Original code (found in the pico examples project):
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

const uint LED_PIN = 25;

// Defaults - just what I tested with any legal value is fine
uint CAPTURE_PIN_BASE = 17;
uint CAPTURE_PIN_COUNT = 2;
uint CAPTURE_N_SAMPLES = 200000;
float FREQ_DIV = 125.0f; // Divide 125Mhz by this to get your freq
uint FREQUENCY = 1000000;
bool TRIGGER = false; // true = high : false = low


uint offset;
struct pio_program *capture_prog_2;

void logic_analyser_init(PIO pio, uint sm, uint pin_base, uint pin_count, float div) {
    // Load a program to capture n pins. This is just a single `in pins, n`
    // instruction with a wrap.
    uint16_t capture_prog_instr = pio_encode_in(pio_pins, pin_count);
    struct pio_program capture_prog = {
        .instructions = &capture_prog_instr,
        .length = 1,
        .origin = -1
    };
    capture_prog_2 = &capture_prog;

    offset = pio_add_program(pio, &capture_prog);

    // Configure state machine to loop over this `in` instruction forever,
    // with autopush enabled.
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_wrap(&c, offset, offset);
    sm_config_set_clkdiv(&c, div);
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_init(pio, sm, offset, &c);
}

void logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words,
                        uint trigger_pin, bool trigger_level) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_chan, &c,
        capture_buf,        // Destinatinon pointer
        &pio->rxf[sm],      // Source pointer
        capture_size_words, // Number of transfers
        true                // Start immediately
    );

    pio_sm_exec(pio, sm, pio_encode_wait_gpio(trigger_level, trigger_pin));
    pio_sm_set_enabled(pio, sm, true);
}

void print_capture_buf_csv(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples) {
    for (int sample = 0; sample < n_samples; ++sample) {
        for (int pin = 0; pin < pin_count; ++pin) {
            uint bit_index = pin + sample * pin_count;
            bool level = !!(buf[bit_index / 32] & 1u << (bit_index % 32));
            printf(level ? "1" : "0");
            printf(",");
        }

        // Blink the LED every 2500 samples to show something is happening
        // Good for a serial capture where you cannot see if it is still outputting
        if ((sample % 5000) == 0)
            gpio_put(LED_PIN, 1);
        else if ((sample % 5000) == 2500)
            gpio_put(LED_PIN, 0);

        printf("\n");
    }
}

void read_user_input() {
    const int BUFFER_MAX = 11;
    char buffer[BUFFER_MAX+1];

    while (true) {
        memset(buffer, 0, BUFFER_MAX+1);
        int bufpos = 0;
        int c = 0;

        while (c != '\r') {
            c = getchar_timeout_us(30000000);
            if (c == -1) continue; // timeout ran out
            if (c == '\r' || c == '\n') break;

            buffer[bufpos++] = (char)c;
            printf("%c", c);
            if (bufpos >= BUFFER_MAX)
                break;
        }
        printf("\n");

        if (buffer[0] == 'p') {
            int pin = -1;
            if (isdigit(buffer[1]) != 0) {
                pin = strtol(buffer+1, NULL, 10);
                if (pin > 28)
                    pin = -1;
            }

            if (pin == -1)
                printf("Pin number is not valid\n");
            else {
                printf("Start pin is %d\n", pin);
                CAPTURE_PIN_BASE = pin;
            }
        }
        else if (buffer[0] == 'n') {
            int number = -1;
            if (isdigit(buffer[1]) != 0) {
                number = strtol(buffer+1, NULL, 10);
                if (number > 28)
                    number = -1;
            }

            if (number == -1)
                printf("Number of pins is not valid\n");
            else {
                printf("Total pins is %d\n", number);
                CAPTURE_PIN_COUNT = number;
            }
        }
        else if (buffer[0] == 'f') {
            uint freq = 0;
            if (isdigit(buffer[1]) != 0) {
                freq = strtol(buffer+1, NULL, 10);
                if (freq > clock_get_hz(clk_sys))
                    freq = 0;
            }

            if (freq < 0)
                printf("Frequency is not valid\n");
            else {
                FREQUENCY = freq;
                FREQ_DIV = clock_get_hz(clk_sys) / (float)FREQUENCY;
                printf("Frequency is %d div is %f\n", FREQUENCY, FREQ_DIV);
            }
        }
        else if (buffer[0] == 't') {
            int t = -1;
            if (buffer[1] == 't' || buffer[1] == '1')
                t = 1;
            else if (buffer[1] == 'f' || buffer[1] == '0')
                t = 0;

            if (t < 0)
                printf("Trigger value is not valid\n");
            else {
                TRIGGER = t;
                printf("Trigger set to %d\n", TRIGGER);
            }
        }
        else if (buffer[0] == 's') {
            int number = -1;
            if (isdigit(buffer[1]) != 0) {
                number = strtol(buffer+1, NULL, 10);
                if (number < 0 || number > 500000)
                    number = -1;
            }

            if (number == -1)
                printf("Sample number is not valid\n");
            else {
                printf("Sample number is %d\n", number);
                CAPTURE_N_SAMPLES = number;
            }
        }
        else if (buffer[0] == 'g') {
            break;
        }
        else {
            printf("Unknown command\n");
            printf("p# - Set the first pin to receive capture data\n");
            printf("n# - Set how many pins to receive capture data\n");
            printf("f# - Set the freqency to capture data at in Hz\n");
            printf("t(1)(0) - Set the trigger to high or low\n");
            printf("    Trigger happens off first pin\n");
            printf("s# - Set how many samples to capture\n");
            printf("g - Go!\n");
        }
    }
}

// Boost the baud rate to try to get the data out faster
// Probably should just call the init with the baud rate option set
#undef PICO_DEFAULT_UART_BAUD_RATE
#define PICO_DEFAULT_UART_BAUD_RATE 921600

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    uint32_t *capture_buf = 0;

    PIO pio = pio0;
    uint sm = 0;
    uint dma_chan = 0;

    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(LED_PIN, 0);

        read_user_input();

        uint32_t capture_buf_memory_size = (CAPTURE_PIN_COUNT * CAPTURE_N_SAMPLES + 31) / 32 * sizeof(uint32_t);
        capture_buf = malloc(capture_buf_memory_size);
        if (capture_buf == NULL) {
            printf("Error allocating capture buffer size %d\n", capture_buf_memory_size);
        }

        logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, FREQ_DIV);

        uint32_t hz = clock_get_hz(clk_sys);
        printf("Clock speed is   %d\n", hz);
        float caphz = (float)hz/FREQ_DIV;
        printf("Capture speed is %f.2\n", caphz);

        printf("Arming trigger\n");
        gpio_put(LED_PIN, 1);

        logic_analyser_arm(pio, sm, dma_chan, capture_buf,
            (CAPTURE_PIN_COUNT * CAPTURE_N_SAMPLES + 31) / 32,
            CAPTURE_PIN_BASE, TRIGGER);

        dma_channel_wait_for_finish_blocking(dma_chan);

        gpio_put(LED_PIN, 0);
        print_capture_buf_csv(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES);

        pio_remove_program(pio, capture_prog_2, offset);

        free(capture_buf);
    }
}
