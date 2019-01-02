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

Soft323x<> rtc;

static void uart_puts(const char *s);
static void timer1_reset();

/******************************************************************************
 * I2C Interface                                                              *
 ******************************************************************************/

static constexpr uint8_t I2C_IDLE = 0;
static constexpr uint8_t I2C_START = 1;
static constexpr uint8_t I2C_START_OK = 2;
static constexpr uint8_t I2C_HAS_ADDR = 3;
static constexpr uint8_t I2C_SEND_READY = 4;
static constexpr uint8_t I2C_SEND_BYTE = 5;
static constexpr uint8_t I2C_SEND_BYTE_OK = 6;
static constexpr uint8_t I2C_RECV_BYTE = 7;
static constexpr uint8_t I2C_RECV_BYTE_OK = 8;
static constexpr uint8_t I2C_ERR = 255;

#define I2C_MAX_EVENTS 64

struct I2CEvent {
	uint8_t status;
	uint8_t old_state;
	uint8_t next_state;
	uint8_t addr;
	uint8_t data;
};

volatile I2CEvent i2c_events[I2C_MAX_EVENTS];

volatile uint8_t i2c_events_wr_ptr;

volatile uint8_t i2c_events_rd_ptr;

static void i2c_event_queue_push(uint8_t status, uint8_t old_state,
                                 uint8_t next_state, uint8_t addr, uint8_t data)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		const uint8_t ptr = (i2c_events_wr_ptr++) & (I2C_MAX_EVENTS - 1);
		i2c_events[ptr].status = status;
		i2c_events[ptr].old_state = old_state;
		i2c_events[ptr].next_state = next_state;
		i2c_events[ptr].addr = addr;
		i2c_events[ptr].data = data;
	}
}

static I2CEvent i2c_event_queue_pop()
{
	I2CEvent res;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		const uint8_t ptr = (i2c_events_rd_ptr++) & (I2C_MAX_EVENTS - 1);
		res.status = i2c_events[ptr].status;
		res.old_state = i2c_events[ptr].old_state;
		res.next_state = i2c_events[ptr].next_state;
		res.addr = i2c_events[ptr].addr;
		res.data = i2c_events[ptr].data;
	}
	return res;
}

static bool i2c_event_queue_empty()
{
	bool res;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		res = i2c_events_wr_ptr == i2c_events_rd_ptr;
	}

	return res;
}

/**
 * The address the bus master would like to read.
 */
volatile uint8_t i2c_addr;

/**
 * The current byte the bus master would like to write.
 */
volatile uint8_t i2c_data;

volatile uint8_t i2c_status;

static void i2c_ack()
{
	// Enable TWI, clear the TWINT flag, enable address matching, enable TWI
	// interrupts
	TWCR = (1 << TWIE) | (1 << TWEA) | (1 << TWINT) | (1 << TWEN);
}

static void i2c_await_ack()
{
	// Enable TWI, clear the TWINT flag, enable address matching, disable TWI
	// interrupts
	TWCR = (1 << TWEA) | (1 << TWEN);
}

static void i2c_listen(uint8_t addr)
{
	// Reset the internal state
	i2c_addr = 0;
	i2c_data = 0;
	i2c_status = 0;

	// Set the listen address
	TWAR = (addr & 0x7F) << 1;

	// Prepare for incoming data
	i2c_ack();
}

ISR(TWI_vect)
{
	const uint8_t i2c_status_old = i2c_status;
	const uint8_t tw_status_ = TW_STATUS;
	switch (tw_status_) {
		/* Slave receiver (SR): The master tries to write to this device */
		case TW_SR_SLA_ACK:
			i2c_status = I2C_START;
			i2c_addr = 0;
			i2c_await_ack();
			break;
		case TW_SR_DATA_ACK:
			if (i2c_status == I2C_START_OK) {
				i2c_addr = TWDR;
				i2c_status = I2C_HAS_ADDR;
				i2c_ack();  // Does not need to be acknowledged by the main loop
			}
			else if (i2c_status == I2C_HAS_ADDR ||
			         i2c_status == I2C_RECV_BYTE_OK) {
				i2c_data = TWDR;
				i2c_status = I2C_RECV_BYTE;
				i2c_await_ack();
			}
			else {
				i2c_status = I2C_ERR;
				i2c_ack();
			}
			break;
		case TW_SR_STOP:
			if (i2c_status == I2C_HAS_ADDR) {
				i2c_status = I2C_SEND_READY;
			} else if (i2c_status == I2C_RECV_BYTE_OK) {
				i2c_status = I2C_IDLE;
			} else {
				i2c_status = I2C_ERR;
			}
			i2c_ack();
			break;

		/* Slave transmitter (ST): The master tries to read data from this
		   device */
		case TW_ST_SLA_ACK:
		case TW_ST_DATA_ACK:
			if (i2c_status == I2C_SEND_READY || i2c_status == I2C_SEND_BYTE_OK) {
				i2c_status = I2C_SEND_BYTE;
				i2c_await_ack();
			}
			else {
				i2c_status = I2C_ERR;
				i2c_ack();
			}
			break;
		case TW_SR_DATA_NACK:
		case TW_ST_DATA_NACK:
		case TW_ST_LAST_DATA:
			i2c_status = I2C_IDLE;
			i2c_ack();
			break;

		case TW_BUS_ERROR:
			// Reset the bus
			i2c_status = I2C_IDLE;
			TWCR = 0;
			i2c_ack();
			break;

		default:
			i2c_ack();
			break;
	}

	i2c_event_queue_push(tw_status_, i2c_status_old, i2c_status, i2c_addr, i2c_data);

	// XXX

	// Read the current I2C status to a temporary register. Handle any I2C
	// event and acknowledge it
	switch (i2c_status) {
		case I2C_START:
			rtc.update();
			i2c_status = I2C_START_OK;
			i2c_ack();
			break;
		case I2C_RECV_BYTE:
			switch (rtc.i2c_write(i2c_addr, i2c_data)) {
				case rtc.ACTION_RESET_TIMER:
					timer1_reset();
					break;
					// TODO
					break;
			}
			i2c_addr = rtc.i2c_next_addr(i2c_addr);
			i2c_status = I2C_RECV_BYTE_OK;
			i2c_ack();
			break;
		case I2C_SEND_BYTE:
			TWDR = rtc.i2c_read(i2c_addr);
			i2c_addr = rtc.i2c_next_addr(i2c_addr);
			i2c_status = I2C_SEND_BYTE_OK;
			i2c_ack();
			break;
	}

	// XXX
}

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
 * UART                                                                       *
 ******************************************************************************/

#define UBRR_VAL ((F_CPU + BAUD * 8) / (BAUD * 16) - 1)
#define BAUD_REAL (F_CPU / (16 * (UBRR_VAL + 1)))
#define BAUD_ERROR ((BAUD_REAL * 1000) / BAUD)

#if ((BAUD_ERROR < 990) || (BAUD_ERROR > 1010))
#error UART Error too large
#endif

static void uart_init(void)
{
	UBRR0 = UBRR_VALUE;
#if USE_2X
	UCSR0A |= (1 << U2X0);
#else
	UCSR0A &= ~(1 << U2X0);
#endif
	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static int uart_putc(unsigned char c)
{
	while (!(UCSR0A & (1 << UDRE0))) {}
	UDR0 = c;
	return 0;
}

static void uart_puts(const char *s)
{
	while (*s) {
		uart_putc(*s);
		s++;
	}
}

static void uart_put_nibble(uint8_t x) {
	if (x <= 9) {
		uart_putc('0' + x);
	} else {
		uart_putc(('A' - 10) + x);
	}
}

static void uart_put_hex(uint8_t x) {
	uart_put_nibble((x >> 4) & 0x0F);
	uart_put_nibble((x >> 0) & 0x0F);
}

/******************************************************************************
 * MAIN PROGRAM                                                               *
 ******************************************************************************/

static const char *tw_status_str(uint8_t tw_status)
{
	switch (tw_status) {
		case TW_START:
			return "START";
		case TW_REP_START:
			return "RSTART";
		case TW_ST_SLA_ACK:
			return "ST_SLA_ACK";
		case TW_ST_DATA_ACK:
			return "ST_DT_ACK";
		case TW_ST_DATA_NACK:
			return "ST_DT_NACK";
		case TW_ST_LAST_DATA:
			return "ST_LAST_DT";
		case TW_SR_SLA_ACK:
			return "SR_SLA_ACK";
		case TW_SR_GCALL_ACK:
			return "SR_GCALL_ACK";
		case TW_SR_DATA_ACK:
			return "SR_DATA_ACK";
		case TW_SR_DATA_NACK:
			return "SR_DATA_NACK";
		case TW_SR_STOP:
			return "SR_STOP";
		case TW_BUS_ERROR:
			return "BUS_ERROR";
		default:
			return "???";
	}
}

static const char *i2c_status_str(uint8_t i2c_status)
{
	switch (i2c_status) {
		case I2C_IDLE :
			return "IDLE     ";
		case I2C_START:
			return "START    ";
		case I2C_START_OK:
			return "START_OK ";
		case I2C_HAS_ADDR:
			return "HAS_ADDR ";
		case I2C_SEND_READY:
			return "SEND_RDY ";
		case I2C_SEND_BYTE:
			return "SEND_BYTE";
		case I2C_SEND_BYTE_OK:
			return "SEND_BYTE_OK";
		case I2C_RECV_BYTE:
			return "RECV_BYTE";
		case I2C_RECV_BYTE_OK:
			return "RECV_BYTE_OK";
		case I2C_ERR:
			return "ERR      ";
		default:
			return "?????????";
	}
}

int main()
{
	OSCCAL = 180;

	set_sleep_mode(SLEEP_MODE_IDLE);

	// Debug port for blinking LED
	DDRB |= 0x01;
	DDRD |= 0x02;

	// Initialize the UART
	uart_init();

	// Initialize the timer
	timer1_init();

	// Listen on I2C address 0x68 (corresponding to the DS3232)
	i2c_listen(0x68);

	// Enable interrupts
	sei();

	while (true) {
		// Nothing to do, go to sleep
		sleep_mode();

		// Print I2C events for debugging purposes
		while (!i2c_event_queue_empty()) {
			const I2CEvent e = i2c_event_queue_pop();
			uart_puts(tw_status_str(e.status));
			uart_puts(":\t");
			uart_puts(i2c_status_str(e.old_state));
			uart_puts("\t->\t");
			uart_puts(i2c_status_str(e.next_state));
			uart_puts("\t0x");
			uart_put_hex(e.addr);
			uart_puts("\t0x");
			uart_put_hex(e.data);
			uart_puts("\n");
		}

		// Only update the RTC if the I2C bus is not busy at the moment
		if (i2c_status == I2C_IDLE || i2c_status == I2C_ERR) {
			if (rtc.update()) {
				PORTB ^= 0x01;
			}
		}
	}
}
