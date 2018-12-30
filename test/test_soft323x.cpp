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

#include <iostream>

#include <foxen/unittest.h>

#include <soft323x/soft323x.hpp>

/******************************************************************************
 * MAIN                                                                       *
 ******************************************************************************/

void test_initialisation()
{
	Soft323x<> soft323x;  // Initialises to Tuesday, 2019/01/01 00:00
	EXPECT_EQ(2019, soft323x.year());
	EXPECT_EQ(1, soft323x.month());
	EXPECT_EQ(1, soft323x.date());
	EXPECT_EQ(2, soft323x.day());

	EXPECT_EQ(0, soft323x.hours());
	EXPECT_EQ(0, soft323x.minutes());
	EXPECT_EQ(0, soft323x.seconds());
}

void test_is_leap_year()
{
	EXPECT_FALSE(Soft323x<>::is_leap_year(1900));
	EXPECT_TRUE(Soft323x<>::is_leap_year(1904));
	EXPECT_TRUE(Soft323x<>::is_leap_year(2000));
	EXPECT_FALSE(Soft323x<>::is_leap_year(2019));
	EXPECT_TRUE(Soft323x<>::is_leap_year(2020));
	EXPECT_FALSE(Soft323x<>::is_leap_year(2100));
	EXPECT_FALSE(Soft323x<>::is_leap_year(2200));
	EXPECT_FALSE(Soft323x<>::is_leap_year(2300));
	EXPECT_TRUE(Soft323x<>::is_leap_year(2400));
}

void test_number_of_days()
{
	EXPECT_EQ(31, Soft323x<>::number_of_days(1, 2000));
	EXPECT_EQ(29, Soft323x<>::number_of_days(2, 2000));
	EXPECT_EQ(31, Soft323x<>::number_of_days(3, 2000));
	EXPECT_EQ(30, Soft323x<>::number_of_days(4, 2000));
	EXPECT_EQ(31, Soft323x<>::number_of_days(5, 2000));
	EXPECT_EQ(30, Soft323x<>::number_of_days(6, 2000));
	EXPECT_EQ(31, Soft323x<>::number_of_days(7, 2000));
	EXPECT_EQ(31, Soft323x<>::number_of_days(8, 2000));
	EXPECT_EQ(30, Soft323x<>::number_of_days(9, 2000));
	EXPECT_EQ(31, Soft323x<>::number_of_days(10, 2000));
	EXPECT_EQ(30, Soft323x<>::number_of_days(11, 2000));
	EXPECT_EQ(31, Soft323x<>::number_of_days(12, 2000));

	EXPECT_EQ(31, Soft323x<>::number_of_days(1, 2001));
	EXPECT_EQ(28, Soft323x<>::number_of_days(2, 2001));
	EXPECT_EQ(31, Soft323x<>::number_of_days(3, 2001));
	EXPECT_EQ(30, Soft323x<>::number_of_days(4, 2001));
	EXPECT_EQ(31, Soft323x<>::number_of_days(5, 2001));
	EXPECT_EQ(30, Soft323x<>::number_of_days(6, 2001));
	EXPECT_EQ(31, Soft323x<>::number_of_days(7, 2001));
	EXPECT_EQ(31, Soft323x<>::number_of_days(8, 2001));
	EXPECT_EQ(30, Soft323x<>::number_of_days(9, 2001));
	EXPECT_EQ(31, Soft323x<>::number_of_days(10, 2001));
	EXPECT_EQ(30, Soft323x<>::number_of_days(11, 2001));
	EXPECT_EQ(31, Soft323x<>::number_of_days(12, 2001));

	EXPECT_EQ(0, Soft323x<>::number_of_days(0, 2001));
	EXPECT_EQ(0, Soft323x<>::number_of_days(13, 2001));
}

void test_update_24_hours()
{
	Soft323x<> soft323x;  // Initialises to Tuesday, 2019/01/01 00:00

	int day = 2;
	//	for (int year = 2019; year <= 2050; year++) {
	for (int year = 2019; year <= 2020; year++) {
		printf("\rTesting year %d...", year);
		fflush(stdout);
		for (int month = 1; month <= 12; month++) {
			const uint8_t n_days = Soft323x<>::number_of_days(month, year);
			for (int date = 1; date <= n_days; date++, day++) {
				if (day > 7) {
					day = 1;
				}
				for (int hours = 0; hours <= 23; hours++) {
					for (int minutes = 0; minutes <= 59; minutes++) {
						for (int seconds = 0; seconds <= 59; seconds++) {
							ASSERT_EQ(year, soft323x.year());
							ASSERT_EQ(month, soft323x.month());
							ASSERT_EQ(date, soft323x.date());
							ASSERT_EQ(day, soft323x.day());

							ASSERT_EQ(hours, soft323x.hours());
							ASSERT_EQ(minutes, soft323x.minutes());
							ASSERT_EQ(seconds, soft323x.seconds());

							soft323x.tick();
							soft323x.update();
						}
					}
				}
			}
		}
	}
	printf("\n");
}

void test_update_12_hours()
{
	Soft323x<> soft323x;  // Initialises to Tuesday, 2019/01/01 00:00

	int day = 2;

	// Switch to 12-hour mode
	soft323x.i2c_write(soft323x.REG_HOURS,
	                   soft323x.bcd_enc(12) | soft323x.BIT_HOUR_12_HOURS);
	ASSERT_EQ(0, soft323x.hours());

	for (int year = 2019; year <= 2301; year++) {  // This takes quite a long
		printf("\rTesting year %d...", year);
		fflush(stdout);
		for (int month = 1; month <= 12; month++) {
			const uint8_t n_days = Soft323x<>::number_of_days(month, year);
			for (int date = 1; date <= n_days; date++, day++) {
				if (day > 7) {
					day = 1;
				}
				for (int hours = 0; hours <= 23; hours++) {
					for (int minutes = 0; minutes <= 59; minutes++) {
						for (int seconds = 0; seconds <= 59; seconds++) {
							ASSERT_EQ(year, soft323x.year());
							ASSERT_EQ(month, soft323x.month());
							ASSERT_EQ(date, soft323x.date());
							ASSERT_EQ(day, soft323x.day());

							ASSERT_EQ(hours, soft323x.hours());
							ASSERT_EQ(minutes, soft323x.minutes());
							ASSERT_EQ(seconds, soft323x.seconds());

							// Make sure te hour and AM/PM bits are correct
							uint8_t reg_hour =
							    soft323x.i2c_read(soft323x.REG_HOURS);
							ASSERT_EQ(((hours < 12) ? 0 : soft323x.BIT_HOUR_PM),
							          reg_hour & soft323x.BIT_HOUR_PM);
							ASSERT_EQ(soft323x.BIT_HOUR_12_HOURS,
							          reg_hour & soft323x.BIT_HOUR_12_HOURS);

							soft323x.tick();
							soft323x.update();
						}
					}
				}
			}
		}
	}
	printf("\n");
}

void test_write_seconds()
{
	Soft323x<> t;
	EXPECT_EQ(t.ACTION_RESET_TIMER, t.i2c_write(t.REG_SECONDS, t.bcd_enc(42)));
	EXPECT_EQ(42, t.seconds());

	EXPECT_EQ(t.ACTION_RESET_TIMER, t.i2c_write(t.REG_SECONDS, t.bcd_enc(0)));
	EXPECT_EQ(0, t.seconds());

	EXPECT_EQ(t.ACTION_RESET_TIMER, t.i2c_write(t.REG_SECONDS, 0xFF));
	EXPECT_EQ(59, t.seconds());
}

void test_write_minutes()
{
	Soft323x<> t;
	EXPECT_EQ(0, t.i2c_write(t.REG_MINUTES, t.bcd_enc(42)));
	EXPECT_EQ(42, t.minutes());

	EXPECT_EQ(0, t.i2c_write(t.REG_MINUTES, t.bcd_enc(0)));
	EXPECT_EQ(0, t.minutes());

	EXPECT_EQ(0, t.i2c_write(t.REG_MINUTES, 0xFF));
	EXPECT_EQ(59, t.minutes());
}

void test_write_hours()
{
	Soft323x<> t;

	// 24 hour format
	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS, t.bcd_enc(23)));
	EXPECT_EQ(23, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS, t.bcd_enc(24)));
	EXPECT_EQ(23, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS, t.bcd_enc(0)));
	EXPECT_EQ(0, t.hours());

	// 12 hour format
	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS,
	                         t.bcd_enc(12) | t.BIT_HOUR_12_HOURS));  // 12 a.m.
	EXPECT_EQ(0, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS,
	                         t.bcd_enc(13) | t.BIT_HOUR_12_HOURS));  // invalid
	EXPECT_EQ(0, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS,
	                         t.bcd_enc(5) | t.BIT_HOUR_12_HOURS));  // 5 a.m.
	EXPECT_EQ(5, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS, t.bcd_enc(12) | t.BIT_HOUR_12_HOURS |
	                                          t.BIT_HOUR_PM));  // 12 p.m.
	EXPECT_EQ(12, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS, t.bcd_enc(13) | t.BIT_HOUR_12_HOURS |
	                                          t.BIT_HOUR_PM));  // invalid
	EXPECT_EQ(12, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS, t.bcd_enc(5) | t.BIT_HOUR_12_HOURS |
	                                          t.BIT_HOUR_PM));  // 5 p.m.
	EXPECT_EQ(17, t.hours());

	EXPECT_EQ(0, t.i2c_write(t.REG_HOURS, t.bcd_enc(11) | t.BIT_HOUR_12_HOURS |
	                                          t.BIT_HOUR_PM));  // 11 p.m.
	EXPECT_EQ(23, t.hours());
}

void test_write_day()
{
	Soft323x<> t;

	EXPECT_EQ(0, t.i2c_write(t.REG_DAY, t.bcd_enc(0)));
	EXPECT_EQ(1, t.day());

	EXPECT_EQ(0, t.i2c_write(t.REG_DAY, t.bcd_enc(1)));
	EXPECT_EQ(1, t.day());

	EXPECT_EQ(0, t.i2c_write(t.REG_DAY, t.bcd_enc(2)));
	EXPECT_EQ(2, t.day());

	EXPECT_EQ(0, t.i2c_write(t.REG_DAY, t.bcd_enc(7)));
	EXPECT_EQ(7, t.day());

	EXPECT_EQ(0, t.i2c_write(t.REG_DAY, t.bcd_enc(8)));
	EXPECT_EQ(1, t.day());
}

void test_write_date()
{
	Soft323x<> t;

	t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY);

	// Invalid: too small
	EXPECT_EQ(0, t.i2c_write(t.REG_DATE, t.bcd_enc(0)));
	EXPECT_EQ(1, t.date());
	t.update();
	EXPECT_EQ(1, t.date());

	// Invalid: too large
	EXPECT_EQ(0, t.i2c_write(t.REG_DATE, t.bcd_enc(32)));
	EXPECT_EQ(31, t.date());

	// Will be set to "28" after update
	t.update();
	EXPECT_EQ(28, t.date());

	// Valid
	EXPECT_EQ(0, t.i2c_write(t.REG_DATE, t.bcd_enc(12)));
	EXPECT_EQ(12, t.date());
	t.update();
	EXPECT_EQ(12, t.date());

	// Valid
	EXPECT_EQ(0, t.i2c_write(t.REG_DATE, t.bcd_enc(28)));
	EXPECT_EQ(28, t.date());
	t.update();
	EXPECT_EQ(28, t.date());

	// Set year to a leap year
	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, t.bcd_enc(00)));
	EXPECT_EQ(0, t.i2c_write(t.REG_DATE, t.bcd_enc(31)));
	EXPECT_EQ(31, t.date());
	t.update();
	EXPECT_EQ(29, t.date());
}

void test_write_month()
{
	Soft323x<> t;

	// Try to change the date to the 30th
	EXPECT_EQ(0, t.i2c_write(t.REG_DATE, t.bcd_enc(30)));
	EXPECT_EQ(30, t.date());
	t.update();
	EXPECT_EQ(30, t.date());

	// Set month to February with/without the century flag
	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY));
	EXPECT_EQ(2, t.month());
	EXPECT_EQ(2019, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2)));
	EXPECT_EQ(2, t.month());
	EXPECT_EQ(1919, t.year());

	// This should now be the 28th
	t.update();
	EXPECT_EQ(28, t.date());

	// Attempt to write invalid months
	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(0) | t.BIT_MONTH_CENTURY));
	EXPECT_EQ(1, t.month());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(13) | t.BIT_MONTH_CENTURY));
	EXPECT_EQ(12, t.month());
}

void test_write_year()
{
	Soft323x<> t;

	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, t.bcd_enc(1)));
	EXPECT_EQ(2001, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(1)));
	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, t.bcd_enc(1)));
	EXPECT_EQ(1901, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, t.bcd_enc(99)));
	EXPECT_EQ(1999, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, t.bcd_enc(49)));
	EXPECT_EQ(1949, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, 0xFF));
	EXPECT_EQ(1999, t.year());

	// Change the date to 2000/02/29
	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY));
	EXPECT_EQ(0, t.i2c_write(t.REG_DATE, t.bcd_enc(29)));
	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, 0));
	t.update();
	EXPECT_EQ(2000, t.year());
	EXPECT_EQ(2, t.month());
	EXPECT_EQ(29, t.date());

	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, 1));
	EXPECT_EQ(2001, t.year());
	EXPECT_EQ(2, t.month());
	EXPECT_EQ(29, t.date());
	t.update();
	EXPECT_EQ(2001, t.year());
	EXPECT_EQ(2, t.month());
	EXPECT_EQ(28, t.date());

	// Test the century bits
	EXPECT_EQ(0, t.i2c_write(t.REG_YEAR, t.bcd_enc(99)));
	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY0));
	EXPECT_EQ(2099, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY1));
	EXPECT_EQ(2199, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY2));
	EXPECT_EQ(2399, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY2 |
	                                          t.BIT_MONTH_CENTURY0));
	EXPECT_EQ(2499, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY2 |
	                                          t.BIT_MONTH_CENTURY1));
	EXPECT_EQ(2599, t.year());

	EXPECT_EQ(0, t.i2c_write(t.REG_MONTH, t.bcd_enc(2) | t.BIT_MONTH_CENTURY2 |
	                                          t.BIT_MONTH_CENTURY1 |
	                                          t.BIT_MONTH_CENTURY0));
	EXPECT_EQ(2699, t.year());
}

void test_write_ctrl_1()
{
	Soft323x<> t;

	// Initial state
	EXPECT_EQ(t.BIT_CTRL_1_RS1 | t.BIT_CTRL_1_RS2 | t.BIT_CTRL_1_INTCN,
	          t.i2c_read(t.REG_CTRL_1));

	// The conv flag cannot be reset
	EXPECT_EQ(t.ACTION_CONVERT_TEMPERATURE, t.i2c_write(t.REG_CTRL_1, 0xFF));
	EXPECT_EQ(0xFF, t.i2c_read(t.REG_CTRL_1));
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_1, 0x00));
	EXPECT_EQ(t.BIT_CTRL_1_CONV, t.i2c_read(t.REG_CTRL_1));
}

void test_write_ctrl_2()
{
	Soft323x<> t;

	// Initial state
	EXPECT_EQ(t.BIT_CTRL_2_OSF, t.i2c_read(t.REG_CTRL_2));

	// The OSF flag can only be reset, but not set
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
	EXPECT_EQ(0, t.i2c_read(t.REG_CTRL_2));

	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, t.BIT_CTRL_2_OSF));
	EXPECT_EQ(0, t.i2c_read(t.REG_CTRL_2));
}

void test_write_ctrl_3()
{
	Soft323x<> t;

	// Initial state
	EXPECT_EQ(0, t.i2c_read(t.REG_CTRL_3));

	// Only the BIT_CTRL_3_BB_TD flag can be set
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_3, 0xFF));
	EXPECT_EQ(t.BIT_CTRL_3_BB_TD, t.i2c_read(t.REG_CTRL_3));

	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_3, 0x00));
	EXPECT_EQ(0, t.i2c_read(t.REG_CTRL_3));
}

void test_write_aging_offset() {
	Soft323x<> t;

	EXPECT_EQ(0, t.i2c_write(t.REG_AGING_OFFSET, 0xFF));
	EXPECT_EQ(0xFF, t.i2c_read(t.REG_AGING_OFFSET));

	EXPECT_EQ(0, t.i2c_write(t.REG_AGING_OFFSET, 0x00));
	EXPECT_EQ(0x00, t.i2c_read(t.REG_AGING_OFFSET));

	EXPECT_EQ(0, t.i2c_write(t.REG_AGING_OFFSET, 0x88));
	EXPECT_EQ(0x88, t.i2c_read(t.REG_AGING_OFFSET));
}

void test_write_sram () {
	Soft323x<16> t;

	for (int i = t.REG_SRAM; i < t.REG_SRAM + 16; i++) {
		EXPECT_EQ(0, t.i2c_write(i, 0xFF));
		EXPECT_EQ(0xFF, t.i2c_read(i));

		EXPECT_EQ(0, t.i2c_write(i, 0x00));
		EXPECT_EQ(0x00, t.i2c_read(i));

		EXPECT_EQ(0, t.i2c_write(i, 0x88));
		EXPECT_EQ(0x88, t.i2c_read(i));
	}

	for (int i = t.REG_SRAM + 16; i < 256; i++) {
		EXPECT_EQ(0, t.i2c_write(i, 0xFF));
		EXPECT_EQ(0x00, t.i2c_read(i));

		EXPECT_EQ(0, t.i2c_write(i, 0x00));
		EXPECT_EQ(0x00, t.i2c_read(i));

		EXPECT_EQ(0, t.i2c_write(i, 0x88));
		EXPECT_EQ(0x00, t.i2c_read(i));
	}
}

void test_write_temp() {
	Soft323x<> t;

	uint8_t old_msb = t.i2c_read(t.REG_TEMP_MSB);
	uint8_t old_lsb = t.i2c_read(t.REG_TEMP_LSB);

	EXPECT_EQ(0, t.i2c_write(t.REG_TEMP_MSB, 0xAF));
	EXPECT_EQ(0, t.i2c_write(t.REG_TEMP_LSB, 0xAF));

	EXPECT_EQ(old_msb, t.i2c_read(t.REG_TEMP_MSB));
	EXPECT_EQ(old_lsb, t.i2c_read(t.REG_TEMP_LSB));
}

void test_write_alarm_1_every_second()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every second
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_SECONDS, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_MINUTES, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_HOURS, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_DAY_OR_DATE, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_read(t.REG_CTRL_2));

	for (int i = 0; i < 24 * 3600 * 365; i++) {
		t.tick();
		t.update();

		EXPECT_EQ(t.BIT_CTRL_2_A1F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
		EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		EXPECT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_1_seconds_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every minute at ss == 42
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_SECONDS, t.bcd_enc(42)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_MINUTES, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_HOURS, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_DAY_OR_DATE, t.BIT_ALARM_MODE));

	for (int j = 0; j < 60 * 24 * 365; j++) {
		for (int i = 0; i < (j == 0 ? 42 : 60); i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
			t.tick();
			t.update();
		}
		ASSERT_EQ(t.BIT_CTRL_2_A1F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_1_minutes_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every minute at ss == 42
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_SECONDS, t.bcd_enc(42)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_MINUTES, t.bcd_enc(32)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_HOURS, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_DAY_OR_DATE, t.BIT_ALARM_MODE));

	for (int j = 0; j < 24 * 365; j++) {
		for (int i = 0; i < (j == 0 ? 42 + 32 * 60 : 3600); i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
			t.tick();
			t.update();
		}
		ASSERT_EQ(t.BIT_CTRL_2_A1F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_1_hours_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every minute at ss == 42
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_SECONDS, t.bcd_enc(42)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_MINUTES, t.bcd_enc(32)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_HOURS, t.bcd_enc(11)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_DAY_OR_DATE, t.BIT_ALARM_MODE));

	for (int j = 0; j < 365; j++) {
		for (int i = 0; i < (j == 0 ? 42 + 32 * 60 + 11 * 3600 : 24 * 3600);
		     i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
			t.tick();
			t.update();
		}
		ASSERT_EQ(t.BIT_CTRL_2_A1F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_1_day_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every minute at ss == 42
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_SECONDS, t.bcd_enc(42)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_MINUTES, t.bcd_enc(32)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_HOURS, t.bcd_enc(11)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_DAY_OR_DATE,
	                         t.bcd_enc(5) | t.BIT_ALARM_IS_DAY));

	for (int j = 0; j < 60; j++) {
		for (int i = 0; i < (j == 0 ? 42 + 32 * 60 + 11 * 3600 + 3 * 24 * 3600
		                            : 7 * 24 * 3600);
		     i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
			t.tick();
			t.update();
		}
		ASSERT_EQ(t.BIT_CTRL_2_A1F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_1_date_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every minute at ss == 42
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_SECONDS, t.bcd_enc(42)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_MINUTES, t.bcd_enc(32)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_HOURS, t.bcd_enc(11)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_1_DAY_OR_DATE, t.bcd_enc(30)));

	for (int i = 0; i < 42 + 32 * 60 + 11 * 3600 + 29 * 24 * 3600; i++) {
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
		t.tick();
		t.update();
	}
	ASSERT_EQ(t.BIT_CTRL_2_A1F, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
	ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
	ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));

	t.tick();
	t.update();
	ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A1F);
}

void test_write_alarm_2_every_minute()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every second
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_MINUTES, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_HOURS, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_DAY_OR_DATE, t.BIT_ALARM_MODE));

	for (int j = 0; j < 24 * 60 * 365; j++) {
		for (int i = 0; i < 60; i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
			t.tick();
			t.update();
		}

		ASSERT_EQ(t.BIT_CTRL_2_A2F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_2_minutes_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every second
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_MINUTES, t.bcd_enc(52)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_HOURS, t.BIT_ALARM_MODE));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_DAY_OR_DATE, t.BIT_ALARM_MODE));

	for (int j = 0; j < 24 * 365; j++) {
		for (int i = 0; i < (j == 0 ? 52 * 60 : 3600); i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
			t.tick();
			t.update();
		}

		ASSERT_EQ(t.BIT_CTRL_2_A2F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_2_hours_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every second
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_MINUTES, t.bcd_enc(52)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_HOURS, t.bcd_enc(21)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_DAY_OR_DATE, t.BIT_ALARM_MODE));

	for (int j = 0; j < 365; j++) {
		for (int i = 0; i < (j == 0 ? 21 * 3600 + 52 * 60 : 24 * 3600); i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
			t.tick();
			t.update();
		}

		ASSERT_EQ(t.BIT_CTRL_2_A2F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_2_day_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every second
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_MINUTES, t.bcd_enc(52)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_HOURS, t.bcd_enc(21)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_DAY_OR_DATE,
	                         t.bcd_enc(7) | t.BIT_ALARM_IS_DAY));

	for (int j = 0; j < 60; j++) {
		for (int i = 0;
		     i < (j == 0 ? 5 * 24 * 3600 + 21 * 3600 + 52 * 60 : 24 * 7 * 3600);
		     i++) {
			ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
			t.tick();
			t.update();
		}

		ASSERT_EQ(t.BIT_CTRL_2_A2F,
		          t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
		ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
	}
}

void test_write_alarm_2_date_match()
{
	Soft323x<> t;

	// Reset the CTRL_2 register
	EXPECT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));

	// Set an alarm for every second
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_MINUTES, t.bcd_enc(52)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_HOURS, t.bcd_enc(21)));
	EXPECT_EQ(0, t.i2c_write(t.REG_ALARM_2_DAY_OR_DATE, t.bcd_enc(31)));

	for (int i = 0; i < 30 * 24 * 3600 + 21 * 3600 + 52 * 60; i++) {
		ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
		t.tick();
		t.update();
	}

	ASSERT_EQ(t.BIT_CTRL_2_A2F, t.i2c_read(t.REG_CTRL_2) & t.BIT_CTRL_2_A2F);
	ASSERT_EQ(0, t.i2c_write(t.REG_CTRL_2, 0x00));
	ASSERT_EQ(0, t.i2c_read(t.REG_CTRL_2));
}

int main()
{
	RUN(test_initialisation);
	RUN(test_is_leap_year);
	RUN(test_number_of_days);
	RUN(test_update_24_hours);
	RUN(test_update_12_hours);
	RUN(test_write_seconds);
	RUN(test_write_minutes);
	RUN(test_write_hours);
	RUN(test_write_day);
	RUN(test_write_date);
	RUN(test_write_month);
	RUN(test_write_year);
	RUN(test_write_ctrl_1);
	RUN(test_write_ctrl_2);
	RUN(test_write_ctrl_3);
	RUN(test_write_aging_offset);
	RUN(test_write_sram);
	RUN(test_write_temp);
	RUN(test_write_alarm_1_every_second);
	RUN(test_write_alarm_1_seconds_match);
	RUN(test_write_alarm_1_minutes_match);
	RUN(test_write_alarm_1_hours_match);
	RUN(test_write_alarm_1_day_match);
	RUN(test_write_alarm_1_date_match);
	RUN(test_write_alarm_2_every_minute);
	RUN(test_write_alarm_2_minutes_match);
	RUN(test_write_alarm_2_hours_match);
	RUN(test_write_alarm_2_day_match);
	RUN(test_write_alarm_2_date_match);
	DONE;
}
