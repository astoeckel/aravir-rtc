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
 * Provides an (incomplete) software implementation of the DS3232 hardware real
 * time clock. This code is meant to be executed on a microcontroller connected
 * to another host computer via I2C.
 *
 * See https://datasheets.maximintegrated.com/en/ds/DS3232.pdf for more
 * information.
 *
 * @author Andreas Stöckel
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

	/**
	 * Set to true if the date was modified. Correspondingly, we must check the
	 * date for validity, i.e. check whether the entire YYYY/MM/DD triple is
	 * correct.
	 */
	bool m_wrote_date;

	/**************************************************************************
	 * Internal helper functions                                              *
	 **************************************************************************/

	/**
	 * Atomically reads the content of the variable m_ticks and resets it to
	 * zero.
	 *
	 * @return the value of m_ticks before it was reset to zero.
	 */
	uint8_t atomic_consume_ticks()
	{
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
		return ticks;
	}

	/**
	 * Used internally by update() to make sure that the date/month/year
	 * combination is valid.
	 */
	void canonicalise_date() {
		const uint8_t n_days = number_of_days(month(), year());
		m_regs.regs.date = bcd_canon(m_regs.regs.date, bcd_enc(1), bcd_enc(n_days));
	}

	/**
	 * Used internally by update() to increment the time by one second.
	 */
	void increment_time()
	{
		// Shorthand for accessing the time registers
		Registers &t = m_regs.regs;

		// Increment seconds
		if (!increment_bcd(t.seconds, MASK_SECONDS, bcd_enc(59))) {
			return;
		}

		// Increment minutes
		if (!increment_bcd(t.minutes, MASK_MINUTES, bcd_enc(59))) {
			return;
		}

		// Increment the hour. Must distinguish between 12 and 24 hour mode.
		if (t.hours & BIT_HOUR_12_HOURS) {
			// We're in the 12 hours mode. Sigh.
			if (!increment_bcd(t.hours, MASK_HOURS_12_HOURS, bcd_enc(13), 1)) {
				if ((t.hours & MASK_HOURS_12_HOURS) == bcd_enc(13)) {
					// Overflow from 12 -> 1
					t.hours = (t.hours & ~MASK_HOURS_12_HOURS) | bcd_enc(1);
					return;
				} else if ((t.hours & MASK_HOURS_12_HOURS) == bcd_enc(12)) {
					// Flip the PM/AM flag
					t.hours = t.hours ^ BIT_HOUR_PM;
					if (t.hours & BIT_HOUR_PM) {
						// It just became noon. No further overflow happens.
						return;
					}
					// It's 12 a.m. New day! Overflow the day and date.
				}
				else {
					return;
				}
			}
		}
		else {
			// We're in the 24 hours mode. This is sane people's land.
			if (!increment_bcd(t.hours, MASK_HOURS_24_HOURS, bcd_enc(23))) {
				return;
			}
		}

		// A new day has started. Increment the day.
		increment_bcd(t.day, MASK_DAY, bcd_enc(7), 1);

		// Increment the date
		{
			const uint8_t n_days = number_of_days(month(), year());
			if (!increment_bcd(t.date, MASK_DATE, bcd_enc(n_days), 1)) {
				return;
			}
		}

		// Increment the month.
		if (!increment_bcd(t.month, MASK_MONTH, bcd_enc(12), 1)) {
			return;
		}

		// Increment the year. (Play Auld Lang Syne.)
		if (!increment_bcd(t.year, MASK_YEAR, bcd_enc(99))) {
			return;
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

	/**
	 * Checks whether any of the given alarms has expired; if yes, sets the
	 * corresponding flag in the control register. This must be called exactly
	 * once per second for Alarm 1 to work correctly. Note: This code relies on
	 * the optimizer to rearange the computations to actually happen when
	 * assigning values to the local variables "alarm1" and "alarm2".
	 */
	void check_alarms()
	{
		// Shorthand for the registers
		Registers &t = m_regs.regs;

		// Skip all the computation if the alarm flags are already set
		const bool a1f = t.ctrl_2 & BIT_CTRL_2_A1F;
		const bool a2f = t.ctrl_2 & BIT_CTRL_2_A2F;

		// Read the alarm flags
		const bool a1m1 = !!(t.alarm_1_seconds & BIT_ALARM_MODE);
		const bool a1m2 = !!(t.alarm_1_minutes & BIT_ALARM_MODE);
		const bool a1m3 = !!(t.alarm_1_hours & BIT_ALARM_MODE);
		const bool a1m4 = !!(t.alarm_1_day_or_date & BIT_ALARM_MODE);
		const bool a1dy = !!(t.alarm_1_day_or_date & BIT_ALARM_IS_DAY);

		const bool a2m1 = !!(t.alarm_2_minutes & BIT_ALARM_MODE);
		const bool a2m2 = !!(t.alarm_2_hours & BIT_ALARM_MODE);
		const bool a2m3 = !!(t.alarm_2_day_or_date & BIT_ALARM_MODE);
		const bool a2dy = !!(t.alarm_2_day_or_date & BIT_ALARM_IS_DAY);

		// Apply the correct masks to the time values
		const uint8_t ss = t.seconds & MASK_SECONDS;
		const uint8_t mm = t.minutes & MASK_MINUTES;
		const uint8_t hh = t.hours & 0x7F;
		const uint8_t dy = t.day & MASK_DAY;
		const uint8_t dt = t.date & MASK_DATE;

		// Apply the correct masks to the alarm values
		const uint8_t a1_ss = t.alarm_1_seconds & MASK_SECONDS;
		const uint8_t a1_mm = t.alarm_1_minutes & MASK_MINUTES;
		const uint8_t a1_hh = t.alarm_1_hours & 0x7F;
		const uint8_t a1_dy_dt = a1dy ? (t.alarm_1_day_or_date & MASK_DAY)
		                              : (t.alarm_1_day_or_date & MASK_DATE);

		const uint8_t a2_mm = t.alarm_2_minutes & MASK_MINUTES;
		const uint8_t a2_hh = t.alarm_2_hours & 0x7F;
		const uint8_t a2_dy_dt = a2dy ? (t.alarm_2_day_or_date & MASK_DAY)
		                              : (t.alarm_2_day_or_date & MASK_DATE);

		// Compute whether the alarm has triggered
		const bool alarm1 =
		    (!a1f) && (a1m1 || (!a1m1 && (a1_ss == ss))) && (a1m2 || (!a1m2 && (a1_mm == mm))) &&
		    (a1m3 || (!a1m3 && (a1_hh == hh))) && (a1m4 || (!a1m4 && ((a1dy ? dy : dt) == a1_dy_dt)));

		const bool alarm2 = (!a2f) && (ss == 0) && (a2m1 || (!a2m1 && (a2_mm == mm))) &&
		                    (a2m2 || (!a2m2 && (a2_hh == hh))) &&
		                    (a2m3 || (!a2m3 && ((a2dy ? dy : dt) == a2_dy_dt)));

		// TODO: Interrupt handling

		// Update the "alarm fired" flags in the control registers
		if (alarm1) {
			t.ctrl_2 = t.ctrl_2 | BIT_CTRL_2_A1F;
		}
		if (alarm2) {
			t.ctrl_2 = t.ctrl_2 | BIT_CTRL_2_A2F;
		}
	}

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
	 * Clamps the given BCD value to the given min/max values.
	 */
	static constexpr uint8_t bcd_canon(uint8_t value,
	                                   uint8_t min_bcd = bcd_enc(0),
	                                   uint8_t max_bcd = bcd_enc(99))
	{
		const uint8_t msd_max = (max_bcd & 0xF0U);
		const uint8_t lsd_max = (max_bcd & 0x0FU);
		const uint8_t msd_min = (min_bcd & 0xF0U);
		const uint8_t lsd_min = (min_bcd & 0x0FU);
		uint8_t msd = (value & 0xF0U);
		uint8_t lsd = (value & 0x0FU);
		if (msd > msd_max || (msd == msd_max && lsd > lsd_max)) {
			msd = msd_max;
			lsd = lsd_max;
		}
		else if (msd < msd_min || (msd == msd_min && lsd < lsd_min)) {
			msd = msd_min;
			lsd = lsd_min;
		}
		return msd | lsd;
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
	static constexpr uint8_t BIT_ALARM_IS_DAY = 0x40;
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
	static constexpr uint8_t BIT_CTRL_3_BB_TD = 0x01;

	/**
	 * Note: this implementation uses three century bits that encode the century
	 * since 1900 in binary, where BIT_MONTH_CENTURY0 is the LSB and
	 * BIT_MONTH_CENTURY2 is the MSB. This is an extension of the behaviour in
	 * the DS323x devices.
	 */
	static constexpr uint8_t BIT_MONTH_CENTURY0 = 0x80;
	static constexpr uint8_t BIT_MONTH_CENTURY1 = 0x40;
	static constexpr uint8_t BIT_MONTH_CENTURY2 = 0x20;

	static constexpr uint8_t ACTION_RESET_TIMER = 0x01;
	static constexpr uint8_t ACTION_CONVERT_TEMPERATURE = 0x02;

	static constexpr uint8_t REG_SECONDS = 0x00;
	static constexpr uint8_t REG_MINUTES = 0x01;
	static constexpr uint8_t REG_HOURS = 0x02;
	static constexpr uint8_t REG_DAY = 0x03;
	static constexpr uint8_t REG_DATE = 0x04;
	static constexpr uint8_t REG_MONTH = 0x05;
	static constexpr uint8_t REG_YEAR = 0x06;
	static constexpr uint8_t REG_ALARM_1_SECONDS = 0x07;
	static constexpr uint8_t REG_ALARM_1_MINUTES = 0x08;
	static constexpr uint8_t REG_ALARM_1_HOURS = 0x09;
	static constexpr uint8_t REG_ALARM_1_DAY_OR_DATE = 0x0A;
	static constexpr uint8_t REG_ALARM_2_MINUTES = 0x0B;
	static constexpr uint8_t REG_ALARM_2_HOURS = 0x0C;
	static constexpr uint8_t REG_ALARM_2_DAY_OR_DATE = 0x0D;
	static constexpr uint8_t REG_CTRL_1 = 0x0E;
	static constexpr uint8_t REG_CTRL_2 = 0x0F;
	static constexpr uint8_t REG_AGING_OFFSET = 0x10;
	static constexpr uint8_t REG_TEMP_MSB = 0x11;
	static constexpr uint8_t REG_TEMP_LSB = 0x12;
	static constexpr uint8_t REG_CTRL_3 = 0x13;
	static constexpr uint8_t REG_SRAM = 0x14;

	/**************************************************************************
	 * Constructor                                                            *
	 **************************************************************************/

	Soft323x() { reset(); }

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
		}
		else {
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
		// Reset the internal state
		atomic_consume_ticks();
		m_wrote_date = false;

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
		m_regs.regs.aging_offset = 0;
		m_regs.regs.temp_msb = 0xFF;
		m_regs.regs.temp_lsb = 0xC0;
		m_regs.regs.ctrl_3 = 0;
	}

	/**
	 * Marks the current time/date as invalid because the osciallator has been
	 * stopped.
	 */
	void set_oscillator_stop_flag()
	{
		m_regs.regs.ctrl_2 = m_regs.regs.ctrl_2 | BIT_CTRL_2_OSF;
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
		// If the date was modified, make sure that the date is valid. Otherwise
		// strange things will happen while trying to update the time.
		if (m_wrote_date) {
			canonicalise_date();
			m_wrote_date = false;
		}

		// Consume the ticks and increment time in seconds steps
		uint8_t ticks = atomic_consume_ticks();
		for (uint8_t i = 0; i < ticks; i++) {
			increment_time();
			check_alarms();
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
	 * Writes to the given address.
	 */
	uint8_t i2c_write(uint8_t addr, uint8_t value)
	{
		uint8_t res = 0;
		switch (addr) {
			case REG_SECONDS:  // Reg 00h: Seconds
				res |= ACTION_RESET_TIMER;
				// fallthrough
			case REG_ALARM_1_SECONDS:  // Reg 07h: Seconds
				m_regs.mem[addr] =
				    bcd_canon(value & MASK_SECONDS, bcd_enc(0), bcd_enc(59));
				atomic_consume_ticks();
				break;                 // Reset countdown chain
			case REG_MINUTES:          // Reg 01h: Minutes
			case REG_ALARM_1_MINUTES:  // Reg 08h: Alarm 1 Minutes
			case REG_ALARM_2_MINUTES:  // Reg 0Bh: Alarm 2 Minutes
				m_regs.mem[addr] =
				    bcd_canon(value & MASK_MINUTES, bcd_enc(0), bcd_enc(59));
				break;
			case REG_HOURS:            // Reg 02h: Hours
			case REG_ALARM_1_HOURS:    // Reg 09h: Alarm 1 Hours
			case REG_ALARM_2_HOURS: {  // Reg 0Ch: Alarm 2 Hours
				const bool is_12_hour = value & BIT_HOUR_12_HOURS;
				if (is_12_hour) {
					m_regs.mem[addr] = bcd_canon(value & MASK_HOURS_12_HOURS,
					                             bcd_enc(1), bcd_enc(12)) |
					                   BIT_HOUR_12_HOURS | (value & BIT_HOUR_PM);
				}
				else {
					m_regs.mem[addr] = bcd_canon(value & MASK_HOURS_24_HOURS,
					                             bcd_enc(0), bcd_enc(23));
				}
				break;
			}
			case REG_DAY:  // Reg 03h: Day
				m_regs.mem[addr] =
				    bcd_canon(value & MASK_DAY, bcd_enc(1), bcd_enc(7));
				break;
			case REG_DATE:  // Reg 04h: Date
				m_regs.mem[addr] =
				    bcd_canon(value & MASK_DATE, bcd_enc(1), bcd_enc(31));
				m_wrote_date = true;
				break;
			case REG_MONTH:  // Reg 05h: Month
				m_regs.mem[addr] =
				    bcd_canon(value & MASK_MONTH, bcd_enc(1), bcd_enc(12)) |
				    (value & (BIT_MONTH_CENTURY0 | BIT_MONTH_CENTURY1 |
				              BIT_MONTH_CENTURY2));
				m_wrote_date = true;
				break;
			case REG_YEAR:  // Reg 06h: Year
				m_regs.mem[addr] = bcd_canon(value & MASK_YEAR);
				m_wrote_date = true;
				break;
			case REG_ALARM_1_DAY_OR_DATE:    // Reg 09h: Alarm 1 Day/date
			case REG_ALARM_2_DAY_OR_DATE: {  // Reg 0Dh: Alarm 2 Day/date
				const bool is_day = value & BIT_ALARM_IS_DAY;
				if (is_day) {
					m_regs.mem[addr] =
					    bcd_canon(value & MASK_DAY, bcd_enc(1), bcd_enc(7)) |
					    BIT_ALARM_IS_DAY;
				}
				else {
					m_regs.mem[addr] =
					    bcd_canon(value & MASK_DATE, bcd_enc(1), bcd_enc(31));
				}
				break;
			}
			case REG_CTRL_1:  // Reg 0Eh: Control 1
				// Do not reset the CONV flag
				m_regs.mem[addr] = value | (m_regs.mem[addr] & BIT_CTRL_1_CONV);
				if (value & BIT_CTRL_1_CONV) {
					res |= ACTION_CONVERT_TEMPERATURE;
				}
				// TODO: Handle other control flags
				break;
			case REG_CTRL_2:  // Reg 0Fh: Control 2/Status
				// The OSF, A1F, A2F registers can only be set to zero. The BSY
				// register is write-protected.
				m_regs.mem[addr] =
				    (value & ~(BIT_CTRL_2_OSF | BIT_CTRL_2_A1F |
				               BIT_CTRL_2_A2F | BIT_CTRL_2_BSY)) |
				    ((value & m_regs.mem[addr]) &
				     (BIT_CTRL_2_OSF | BIT_CTRL_2_A1F | BIT_CTRL_2_A2F)) |
				    (value & BIT_CTRL_2_BSY);
				break;
			case REG_CTRL_3: // Reg 13h: Control 3
				m_regs.mem[addr] = value & BIT_CTRL_3_BB_TD;
				break;
			case REG_TEMP_MSB: // Reg 11h: Temp MSB
			case REG_TEMP_LSB: // Reg 12h: Temp LSB
				// Read-only
				break;
			case REG_AGING_OFFSET: // Reg 10h: Aging offset
				// Just write to the register bank
			default:  // SRAM
				if (addr < sizeof(m_regs)) {
					m_regs.mem[addr] = value;
				}
				break;
		}

		// Copy the alarm mode flag
		if (addr >= 0x07 && addr <= 0x0D && (value & BIT_ALARM_MODE)) {
			m_regs.mem[addr] = m_regs.mem[addr] | BIT_ALARM_MODE;
		}

		return res;
	}
};
#pragma pack(pop)
