/**
 *  Soft323x -- Software implementation of the DS323x RTC for 8-bit µCs
 *  Copyright (C) 2019  Andreas Stöckel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define BAUD 19200UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>

#include <util/atomic.h>
#include <util/delay.h>
#include <util/setbaud.h>
#include <util/twi.h>

#include <stdint.h>

#include "../soft323x/soft323x.hpp"

/******************************************************************************
 * Global variables                                                           *
 ******************************************************************************/

static Soft323x<> rtc;

/******************************************************************************
 * Timer 1 as second clock                                                    *
 ******************************************************************************/

ISR(TIMER1_COMPA_vect) { rtc.tick(); }

static void timer1_reset()
{
	TCNT1 = 0;  // Reset the counter
}

static void timer1_init()
{
	timer1_reset();
	OCR1A = F_CPU / 256L;    // This is an integer for f_clkCPU = 8Mhz
	TIMSK1 = (1 << OCIE1A);  // Enable overflow interrupt
	TCCR1B = (1 << WGM12) | (1 << CS12);  // CTC mode; f = f_clkCPU / 256
}

/******************************************************************************
 * I2C Interface                                                              *
 ******************************************************************************/

static constexpr uint8_t I2C_IDLE = 0;
static constexpr uint8_t I2C_START = 1;
static constexpr uint8_t I2C_HAS_ADDR = 2;
static constexpr uint8_t I2C_SEND_READY = 3;
static constexpr uint8_t I2C_SEND_BYTE = 4;
static constexpr uint8_t I2C_RECV_BYTE = 5;

/**
 * The address the bus master would like to read.
 */
volatile uint8_t i2c_addr;

/**
 * The status of the I2C statemachine.
 */
volatile uint8_t i2c_status;

static void i2c_ack()
{
	// Enable TWI, clear the TWINT flag, enable address matching, enable TWI
	// interrupts
	TWCR = (1 << TWIE) | (1 << TWEA) | (1 << TWINT) | (1 << TWEN);
}

static void i2c_listen(uint8_t addr)
{
	// Reset the internal state
	i2c_addr = 0;
	i2c_status = 0;

	// Set the listen address
	TWAR = (addr & 0x7F) << 1;

	// Prepare for incoming data
	i2c_ack();
}

static uint8_t i2c_state_machine(uint8_t tw_status) {
	switch (tw_status) {
		/* Slave receiver (SR): The master tries to write to this device */
		case TW_SR_SLA_ACK:
			i2c_addr = 0;
			rtc.update();
			return I2C_START;
		case TW_SR_DATA_ACK:
			if (i2c_status == I2C_START) {
				i2c_addr = TWDR;
				return I2C_HAS_ADDR;
			}
			else if (i2c_status == I2C_HAS_ADDR ||
			         i2c_status == I2C_RECV_BYTE) {
				switch (rtc.i2c_write(i2c_addr, TWDR)) {
					case rtc.ACTION_RESET_TIMER:
						timer1_reset();
						break;
						// TODO
						break;
				}
				i2c_addr = rtc.i2c_next_addr(i2c_addr);
				return I2C_RECV_BYTE;
			}
			break;
		case TW_SR_STOP:
			if (i2c_status == I2C_HAS_ADDR) {
				return I2C_SEND_READY;
			}
			break;

		/* Slave transmitter (ST): The master tries to read data from this
		   device */
		case TW_ST_SLA_ACK:
		case TW_ST_DATA_ACK:
			if (i2c_status == I2C_SEND_READY || i2c_status == I2C_SEND_BYTE) {
				TWDR = rtc.i2c_read(i2c_addr);
				i2c_addr = rtc.i2c_next_addr(i2c_addr);
				return I2C_SEND_BYTE;
			}
			break;

		case TW_BUS_ERROR:
			// Reset the bus
			TWCR = 0;
			break;

	}

	return I2C_IDLE;
}

ISR(TWI_vect)
{
	i2c_status = i2c_state_machine(TW_STATUS);
	i2c_ack();
}

/******************************************************************************
 * MAIN PROGRAM                                                               *
 ******************************************************************************/

int main()
{
	// Calibrate the internal oscillator (this will be a different value for
	// each individual AVR; prefer using an external clock crystal).
	OSCCAL = 180;

	set_sleep_mode(SLEEP_MODE_IDLE);

	// Debug port for blinking LED
	DDRB |= 0x01;

	// Initialize the timer
	timer1_init();

	// Listen on I2C address 0x68 (corresponding to the DS3232)
	i2c_listen(0x68);

	// Enable interrupts
	sei();

	while (true) {
		// Nothing to do, go to sleep
		sleep_mode();

		// Only update the RTC if the I2C bus is not busy at the moment
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			if (i2c_status == I2C_IDLE) {
				if (rtc.update()) {
					PORTB ^= 0x01; // Toggle an LED
				}
			}
		}
	}
}
