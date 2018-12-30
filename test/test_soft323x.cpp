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

void test_initialisation() {
	Soft323x<> soft323x; // Initialises to Tuesday, 2019/01/01 00:00
	EXPECT_EQ(2019, soft323x.year());
	EXPECT_EQ(1, soft323x.month());
	EXPECT_EQ(1, soft323x.date());
	EXPECT_EQ(2, soft323x.day());

	EXPECT_EQ(0, soft323x.hours());
	EXPECT_EQ(0, soft323x.minutes());
	EXPECT_EQ(0, soft323x.seconds());
}

void test_is_leap_year() {
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

void test_number_of_days() {
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
}

void test_update_24_hours() {
	Soft323x<> soft323x; // Initialises to Tuesday, 2019/01/01 00:00

	int day = 2;
//	for (int year = 2019; year <= 2050; year++) { // This takes quite a long time
	for (int year = 2019; year <= 2025; year++) {
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

int main() {
	RUN(test_initialisation);
	RUN(test_is_leap_year);
	RUN(test_number_of_days);
	RUN(test_update_24_hours);
	DONE;
}
