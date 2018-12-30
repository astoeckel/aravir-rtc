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

/**
 * Provides a software implementation of the DS3232 hardware real time clock.
 * This code is meant to be executed on a microcontroller connected to another
 * host computer via I2C.
 *
 * @author https://datasheets.maximintegrated.com/en/ds/DS3232.pdf
 */

#if __AVR__
#include <util/atomic.h>
#else
#include <atomic>
#endif

#pragma pack(push, 1)
/**
 * A software implementation of the DS3232 hardware realtime clock. This code
 * is mostly platform agnostic but designed to run on something like an 8-bit
 * AVR microcontroller connected to a Raspberry Pi or similary via I2C. The
 * Linux kernel supports the DS3232 out of the box, so now additional driver
 * code is required.
 *
 * The general usage pattern is to execute the tick() function in an interrupt
 * service routine once per second. Then, the program main loop may update the
 * actual time by calling update() if the chip is not accessed via I2C at the
 * moment.
 *
 * @tparam SRAM_SIZE is the size of the user-exposed SRAM in bytes. For a DS3232
 * this value should be 236, for a DS3231 it should be 0.
 */
template <unsigned int SRAM_SIZE = 0>
class Soft323x {
private:
	/**************************************************************************
	 * Private member variables and types                                     *
	 **************************************************************************/

	/**
	 * Content of the virtual DS3232 registers.
	 */
	struct Registers {
		uint8_t seconds;  // Reg 00h
		uint8_t minutes;  // Reg 01h
		uint8_t hours;    // Reg 02h
		uint8_t day;      // Reg 03h
		uint8_t date;     // Reg 04h
		uint8_t month;    // Reg 05h
		uint8_t year;     // Reg 06h

		uint8_t alarm_1_seconds;      // Reg 07h
		uint8_t alarm_1_minutes;      // Reg 08h
		uint8_t alarm_1_hours;        // Reg 09h
		uint8_t alarm_1_day_or_date;  // Reg 0Ah

		uint8_t alarm_2_minutes;      // Reg 0Bh
		uint8_t alarm_2_hours;        // Reg 0Ch
		uint8_t alarm_2_day_or_date;  // Reg 0Dh

		uint8_t ctrl_1;  // Reg 0Eh
		uint8_t ctrl_2;  // Reg 0Fh

		uint8_t aging_offset;  // Reg 10h
		uint8_t temp_msb;      // Reg 11h
		uint8_t temp_lsb;      // Reg 12h

		uint8_t ctrl_3;  // Reg 13h

		uint8_t sram[SRAM_SIZE];  // Reg 14h-FFh
	};

	/**
	 * Register set as exposed to the I2C bus. Note that on read the time is
	 * copied to a secondary register set m_time and read from there.
	 */
	union {
		Registers regs;
		uint8_t mem[sizeof(Registers)];
	} m_regs;

	/**
	 * Buffer containing the number of ticks that passed since the last call to
	 * update().
	 */
#if __AVR__
	volatile uint8_t m_ticks;
#else
	std::atomic<uint8_t> m_ticks;
#endif

public:
	/**************************************************************************
	 * Time and date utility functions                                        *
	 **************************************************************************/

	/**
	 * Used internally to encode a binary value as a binary coded decimal (BCD).
	 * Note that this code does not use division, since the hardware we're
	 * running on may not support hardware division.
	 *
	 * @param is the binary value that should be encoded as BCD. Must be <= 99,
	 * otherwise the result is invalid.
	 * @return the encoded BCD value.
	 */
	static constexpr uint8_t bcd_enc(uint8_t value)
	{
		uint8_t lsd = value;  // Least-significant digit
		uint8_t msd = 0U;     // Most-significant digit
		if (lsd >= 80U) {
			lsd -= 80U;
			msd += 8U;
		}
		if (lsd >= 40U) {
			lsd -= 40U;
			msd += 4U;
		}
		if (lsd >= 20U) {
			lsd -= 20U;
			msd += 2U;
		}
		if (lsd >= 10U) {
			lsd -= 10U;
			msd += 1U;
		}
		return (msd << 4U) | lsd;
	}

	/**
	 * Decodes the given BCD value to a binary number. Proof showing that this
	 * code should be correct. Let X be the BCD-coded number and Y the original
	 * number; |_ _| denotes rounding down. Then for Y < 100
	 *
	 *     |   Y  |        |   Y   |   Y  |      |
	 * X = |  __  | * 16 + |  -- - |  --  | * 10 |
	 *     |_ 10 _|        |_ 10   |_ 10 _|     _|
	 *
	 * In particular, note that
	 *
	 *     |   Y  |
	 * X - |  __  | * 6 = Y
	 *     |_ 10 _|
	 *
	 * because (16 - 6) = 10.
	 *
	 * @param value is the BCD coded value that should be decoded to binary. The
	 * value represented by the BCD must be smaller than 99.
	 * @return the BCD coded decimal.
	 */
	static constexpr uint8_t bcd_dec(uint8_t value)
	{
		// See https://stackoverflow.com/a/42340213
		return value - 6U * (value >> 4U);
	}

	/**
	 * Function used to increment a BCD value "reg" up to a maximum value,
	 * overflowing to the specified value. Assumes that the BCD value is valid.
	 *
	 * @param reg is the register holding the BCD value that should be updated.
	 * Assumes that the BCD value is stored in the least significant bits.
	 * @param mask is a bit mask that should be applied to the register to
	 * extract the actual BCD value.
	 * @param max_bcd is the maximum value, encoded as BCD, that can be stored
	 * in the register.
	 * @param overflow_to_bcd if the increment operation causes an overflow
	 * beyond the specified maximum, the register is set to this value.
	 * @return true if there was an overflow to the given value, false if not.
	 * The return value is used to propagate overflows through the
	 * hours/minutes/seconds chain.
	 */
	static bool increment_bcd(uint8_t &reg, uint8_t mask, uint8_t max_bcd,
	                          uint8_t overflow_to_bcd = 0)
	{
		// Extract the current bcd value from the register
		uint8_t bcd = reg & mask;

		// Handle overflows -- if we're already at the maximum value, just go
		// to the specified overflow value.
		const bool overflow = bcd == max_bcd;
		if (overflow) {
			bcd = overflow_to_bcd;
		}
		else {
			// Increment the BCD value and canonicalise if the last digit
			// overflows
			bcd++;
			if ((bcd & 0x0F) >= 0x0A) {
				bcd = (bcd & 0xF0) + 0x10;
			}
		}

		// Write the modified bcd value back to the register
		reg = (reg & (~mask)) | (bcd & mask);

		return overflow;
	}

	/**
	 * Old Greg's leap year rule.
	 *
	 * @param year is the year for which we want to determine whether or not
	 * this is a leap year. Year is relative to the beginning of the epoch.
	 * @return true if the given year is a leap year, false otherwise.
	 */
	static constexpr bool is_leap_year(uint16_t year)
	{
		return (year % 4U == 0U) && ((year % 100U != 0) || (year % 400U == 0));
	}

	/**
	 * For the given month/year combination computes how many days there are in
	 * this month.
	 *
	 * @param month is the month for which the number of days should be
	 * computed. Must be a value between 1 and 31.
	 * @param year is the year corresponding to the month for which the year
	 * should be computed. Only relevant for leap years and Februrary.
	 */
	static constexpr uint8_t number_of_days(uint8_t month, uint16_t year)
	{
		// Courtesy of the knuckle rule. See
		// https://happyhooligans.ca/trick-to-remember-which-months-have-31-days/
		// I hope my hand knuckles are anatomically normal.
		switch (month) {
			case 1:
				return 31;
			case 2:
				if (is_leap_year(year)) {
					return 29;
				}
				return 28;
			case 3:
				return 31;
			case 4:
				return 30;
			case 5:
				return 31;
			case 6:
				return 30;
			case 7:
				return 31;
			case 8:
				return 31;
			case 9:
				return 30;
			case 10:
				return 31;
			case 11:
				return 30;
			case 12:
				return 31;
			default:
				return 0;
		}
	}

	/**************************************************************************
	 * Public constants (see datasheets)                                      *
	 **************************************************************************/

	static constexpr uint8_t MASK_SECONDS = 0x7F;
	static constexpr uint8_t MASK_MINUTES = 0x7F;
	static constexpr uint8_t MASK_HOURS_12_HOURS = 0x01F;
	static constexpr uint8_t MASK_HOURS_24_HOURS = 0x03F;
	static constexpr uint8_t MASK_DAY = 0x07;
	static constexpr uint8_t MASK_DATE = 0x3F;
	static constexpr uint8_t MASK_MONTH = 0x1F;
	static constexpr uint8_t MASK_YEAR = 0xFF;

	static constexpr uint8_t BIT_HOUR_12_HOURS = 0x40;
	static constexpr uint8_t BIT_HOUR_PM = 0x20;
	static constexpr uint8_t BIT_MONTH_CENTURY = 0x80;
	static constexpr uint8_t BIT_ALARM_MODE = 0x80;
	static constexpr uint8_t BIT_CTRL_1_EOSC = 0x80;
	static constexpr uint8_t BIT_CTRL_1_BBSQW = 0x40;
	static constexpr uint8_t BIT_CTRL_1_CONV = 0x20;
	static constexpr uint8_t BIT_CTRL_1_RS2 = 0x10;
	static constexpr uint8_t BIT_CTRL_1_RS1 = 0x08;
	static constexpr uint8_t BIT_CTRL_1_INTCN = 0x04;
	static constexpr uint8_t BIT_CTRL_1_A2I1 = 0x02;
	static constexpr uint8_t BIT_CTRL_1_A1IE = 0x01;
	static constexpr uint8_t BIT_CTRL_2_OSF = 0x80;
	static constexpr uint8_t BIT_CTRL_2_BB32KHZ = 0x40;
	static constexpr uint8_t BIT_CTRL_2_CRATE1 = 0x20;
	static constexpr uint8_t BIT_CTRL_2_CRATE0 = 0x10;
	static constexpr uint8_t BIT_CTRL_2_EN32KHZ = 0x08;
	static constexpr uint8_t BIT_CTRL_2_BSY = 0x04;
	static constexpr uint8_t BIT_CTRL_2_A2F = 0x02;
	static constexpr uint8_t BIT_CTRL_2_A1F = 0x01;
	static constexpr uint8_t BIT_CTRL_4_BB_TD = 0x01;

	/**
	 * Note: this implementation uses three century bits that encode the century
	 * since 1900 in binary, where BIT_MONTH_CENTURY0 is the LSB and
	 * BIT_MONTH_CENTURY2 is the MSB.
	 */
	static constexpr uint8_t BIT_MONTH_CENTURY0 = 0x80;
	static constexpr uint8_t BIT_MONTH_CENTURY1 = 0x40;
	static constexpr uint8_t BIT_MONTH_CENTURY2 = 0x20;

	/**************************************************************************
	 * Constructor                                                            *
	 **************************************************************************/

	Soft323x() : m_ticks(0) {
		reset();
	}

	/**************************************************************************
	 * Time/date API                                                          *
	 **************************************************************************/

	/**
	 * Returns the current time in seconds.
	 */
	uint8_t seconds() const
	{
		return bcd_dec(m_regs.regs.seconds & MASK_SECONDS);
	}

	/**
	 * Returns the current time in minutes.
	 */
	uint8_t minutes() const
	{
		return bcd_dec(m_regs.regs.minutes & MASK_MINUTES);
	}

	/**
	 * Returns the current hour in the 24 hour format, even if the date is
	 * stored in the 12 hour format internally.
	 */
	uint8_t hours() const
	{
		const Registers &t = m_regs.regs;
		if (t.hours & BIT_HOUR_12_HOURS) {
			const uint8_t h = bcd_dec(t.hours & MASK_HOURS_12_HOURS);
			if (t.hours & BIT_HOUR_PM) {
				if (h == 12U) {
					return h;
				}
				return 12U + h;
			}
			else {
				if (h == 12U) {
					return 0U;
				}
				return h;
			}
		} else {
			return bcd_dec(t.hours & MASK_HOURS_24_HOURS);
		}
	}

	/**
	 * Returns the current day of the week as a number between 1 and 7. The
	 * meaning of this field is user-defined; but a good convention is to treat
	 * Monday as "1".
	 */
	uint8_t day() const { return bcd_dec(m_regs.regs.day & MASK_DAY); }

	/**
	 * Returns the current date as a value between 1 and 31.
	 */
	uint8_t date() const { return bcd_dec(m_regs.regs.date & MASK_DATE); }

	/**
	 * Returns the current month as a value between 1 and 12.
	 */
	uint8_t month() const { return bcd_dec(m_regs.regs.month & MASK_MONTH); }

	/**
	 * Returns the current year assuming that a century value of "0" corresponds
	 * to the year 1900, and a value of "1" to the year 2000. Since this code
	 * was written in the 2010's the correct offset doesn't really matter to me
	 * for leap-year computation w.r.t. to the 100/400 years rule. Sorry.
	 */
	uint16_t year() const
	{
		const Registers &t = m_regs.regs;
		uint16_t year = 1900 + bcd_dec(t.year & MASK_YEAR);
		if (t.month & BIT_MONTH_CENTURY0) {
			year += 100;
		}
		if (t.month & BIT_MONTH_CENTURY1) {
			year += 200;
		}
		if (t.month & BIT_MONTH_CENTURY2) {
			year += 400;
		}
		return year;
	}

	/**************************************************************************
	 * Control API                                                            *
	 **************************************************************************/

	/**
	 * Resets the real-time clock to its initial state. This includes the time
	 * information. Please reset the second timer before calling this function.
	 */
	void reset()
	{
		// Reset the date to 2019/01/01 at 00:00:00.
		m_regs.regs.seconds = bcd_enc(0);
		m_regs.regs.minutes = bcd_enc(0);
		m_regs.regs.hours = bcd_enc(0);
		m_regs.regs.day = bcd_enc(2);
		m_regs.regs.date = bcd_enc(1);
		m_regs.regs.month = bcd_enc(1) | BIT_MONTH_CENTURY;
		m_regs.regs.year = bcd_enc(19);

		// Reset the alarms
		m_regs.regs.alarm_1_seconds = bcd_enc(0);
		m_regs.regs.alarm_1_minutes = bcd_enc(0);
		m_regs.regs.alarm_1_hours = bcd_enc(0);
		m_regs.regs.alarm_1_day_or_date = bcd_enc(1);

		m_regs.regs.alarm_2_minutes = bcd_enc(0);
		m_regs.regs.alarm_2_hours = bcd_enc(0);
		m_regs.regs.alarm_2_day_or_date = bcd_enc(1);

		// Reset the control words
		m_regs.regs.ctrl_1 = BIT_CTRL_1_RS2 | BIT_CTRL_1_RS1 | BIT_CTRL_1_INTCN;
		m_regs.regs.ctrl_2 = BIT_CTRL_2_OSF;
		m_regs.regs.aging_offset = bcd_enc(0);
		m_regs.regs.temp_msb = bcd_enc(0);
		m_regs.regs.temp_lsb = bcd_enc(0);
	}

	/**
	 * Updates the time by one second. This function is designed to be called
	 * from an ISR. Assuming that writes to uint8_t are atomic (which is true
	 * for most µCs), this function is safe to call while the other functions
	 * are active. To commit the update to memory, make sure to call the
	 * update() function. You must ensure that update() is called at least
	 * every 255 seconds.
	 */
	void tick() { m_ticks++; }

	/**
	 * Commits all ticks collected so far. This function must be called
	 * exactly if
	 *
	 * - tick() might have been called and the I2C bus is not active,
	 * - an I2C start word is received, the bus is getting active,
	 * - the I2C bus is active and the read address wraps to address zero.
	 *
	 * Do not call this function if any of the above conditions is not met.
	 * Especially, do not call this function if the I2C bus is active and the
	 * read address does not wrap.
	 *
	 * You must ensure that this function is called at least every 255 seconds!
	 */
	void update()
	{
		// Shorthand for accessing the time registers
		Registers &t = m_regs.regs;

		// Atomically read the number of queued ticks and reset the number of
		// queued ticks to zero
		uint8_t ticks;
#if __AVR__
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			ticks = m_ticks;
			m_ticks = 0U;
		}
#else
		ticks = m_ticks.exchange(0U);
#endif

		// Consume the ticks
		for (uint8_t i = 0; i < ticks; i++) {
			// Increment seconds
			if (!increment_bcd(t.seconds, MASK_SECONDS, bcd_enc(59))) {
				continue;
			}

			// Increment minutes
			if (!increment_bcd(t.minutes, MASK_MINUTES, bcd_enc(59))) {
				continue;
			}

			// Increment the hour. Must distinguish between 12 and 24 hour mode.
			if (t.hours & BIT_HOUR_12_HOURS) {
				// We're in the 12 hours mode. Sigh. Grow up, suckers! This is
				// complicated shit.
				if (!increment_bcd(t.hours, MASK_HOURS_12_HOURS, bcd_enc(12),
				                   1)) {
					if ((t.hours & MASK_HOURS_12_HOURS) == 12) {
						// Flip the PM/AM flag
						t.hours = t.hours ^ BIT_HOUR_PM;
						if (t.hours & BIT_HOUR_PM) {
							// It just became noon. No further overflow happens.
							continue;
						}
						// It's 12 a.m. New day! Overflow the day and date.
					}
					else {
						continue;
					}
				}
			}
			else {
				// We're in the 24 hours mode. This is sane people's land.
				if (!increment_bcd(t.hours, MASK_HOURS_24_HOURS, bcd_enc(23))) {
					continue;
				}
			}

			// A new day has started. Increment the day.
			increment_bcd(t.day, MASK_DAY, bcd_enc(7), 1);

			// Increment the date
			{
				const uint8_t n_days = number_of_days(month(), year());
				if (!increment_bcd(t.date, MASK_DATE, bcd_enc(n_days), 1)) {
					continue;
				}
			}

			// Increment the month.
			if (!increment_bcd(t.month, MASK_MONTH, bcd_enc(12), 1)) {
				continue;
			}

			// Increment the year. (Play Auld Lang Syne.)
			if (!increment_bcd(t.year, MASK_YEAR, bcd_enc(99))) {
				continue;
			}

			// Huzzah! A new century hath begun. (Toggle the century bits.)
			t.month = t.month ^ BIT_MONTH_CENTURY0;
			if (!(t.month & BIT_MONTH_CENTURY0)) {
				t.month = t.month ^ BIT_MONTH_CENTURY1;
				if (!(t.month & BIT_MONTH_CENTURY1)) {
					t.month = t.month ^ BIT_MONTH_CENTURY2;
					// No more bits to overflow to. Sorry people of the future.
				}
			}
		}
	}

	/**************************************************************************
	 * I2C Interface                                                          *
	 **************************************************************************/

	/**
	 * Reads the byte stored at the given address.
	 *
	 * @param addr is the address that should be read. Must be between
	 */
	uint8_t i2c_read(uint8_t addr) const
	{
		// Make sure the read is not out of bounds
		if (addr >= sizeof(Registers)) {
			return 0U;
		}

		// Return the memory content at the given address
		return m_regs.mem[addr];
	}

	/**
	 * Writes to the given address. If this function
	 */
	bool i2c_write(uint8_t addr, uint8_t value) {}
};
#pragma pack(pop)
