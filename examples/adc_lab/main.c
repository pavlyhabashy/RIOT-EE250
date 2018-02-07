/*
 * Copyright (C) 2014-2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example code for accessing an ADC adapted from RIOT's ADC test
 *
 * @author      Jason A. Tran <jasontra@usc.edu>
 *
 * @}
 */

#include <stdio.h>

#include "xtimer.h"
#include "timex.h"
#include "periph/adc.h"

/**
 * ADC_RES_#BIT is defined in many possible places depending on ADC's supported 
 * resolutions. Since the CC2538 SoC has an internal ADC, you can find the
 * supported resolutions in "$(RIOT_BASE)/cpu/cc2538/include/periph_cpu.h".
 */
#define RESOLUTION      ADC_RES_7BIT       

#define DELAY           (100LU * US_PER_MS) /* 100 ms, US_PER_MS defined in timex.h*/

int main(void)
{
    /* get current timestamp */
    xtimer_ticks32_t last = xtimer_now();

    /* always good to initialize a variable to 0 */ 
    int sample = 0; 

    puts("\nEE 250 Lab 02: ADC and Kernel Hacking\n");

    /**
     * Initialize only ADC line 0 which corresponds to the Cortex M3's Port A
     * Pin 3, or "PA2" on a schematic. This corresponds to the AD4/DIO4 pin on
     * the OpenMote's XBee header. We figured this out by reading the ADC 
     * drivers and cross-referencing it with OpenMote schematics.
     */
    if (adc_init(ADC_LINE(0)) < 0) {
        printf("Initialization of ADC_LINE(0) failed\n");
        return 1;
    } else {
        printf("Successfully initialized ADC_LINE(0)\n");
    }

    while (1) {
        sample = adc_sample(ADC_LINE(0), RESOLUTION);

        if (sample < 0) {
            printf("Error: resolution not supported?\n");
        } else {
            printf("ADC_LINE(0): %i\n", sample);
        }

        /** 
         * This function puts the thread to sleep and sets an timer interrupt to 
         * wakeup trigger after DELAY microseconds. When the interrupt triggers,
         * the kernel wakes up this thread. This method is more accurate than 
         * xtimer_sleep() since it makes sure the thread wakes up exactly DELAY 
         * microseconds since the last time you called this function. It 
         * accounts for the execution time of lines 51-57.
         */
        xtimer_periodic_wakeup(&last, DELAY);
    }

    /* this should never be reached */
    return 0;
}
