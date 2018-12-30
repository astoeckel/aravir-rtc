# Soft323x ‒ Software RTC Implemementation for AVRs

Platform-independent software implementation of the popular Maxim Integrated DS323x series real-time clock ICs for use on 8-bit microcontrollers, such as Microchip AVRs. The intended use-case is to add a real-time clock to single board computer such as a Raspberry Pi that is already connected to an AVR with battery backup via I²C. Simulating the DS323x in software on the AVR means that the Linux kernel modules for these chips can be used.

## Compatibility with the hardware modules

A table of all available Maxim Integrated real-time clock ICs can be found at the end of [this Application Note](https://www.maximintegrated.com/en/app-notes/index.mvp/id/504)

This library implements a BCD coded real-time clock with two alarms and was primarily modelled after the DS3232 chip. As of now, it does not provide functionality such as interrupts and square-wave generation, as those were not required for the intended use-case of this library. Feel free to contribute corresponding code.

The library should be more-or-less compatible with the following I²C hardware ICs:

* [DS1337](https://www.maximintegrated.com/en/products/DS1337)
* [DS1338](https://www.maximintegrated.com/en/products/DS1338)
* [DS1339](https://www.maximintegrated.com/en/products/DS1339)
* [DS3231](https://www.maximintegrated.com/en/products/DS3231)
* [DS3232](https://www.maximintegrated.com/en/products/DS3232)

Note that this code is not intended to be a perfect replacement for the hardware chips, it mainly focuses on what is required by the Linux driver. You will need to invest a lot of effort to actually implement an accurate second-clock source to drive this library, so depending on your requirements, you should just go for one of the hardware chips!

### Extensions

The following (mostly compatible) extensions were implemented:

* There are three century bits instead of just one, encoding years up to 2699.
* Leap years are guaranteed to be correct up to the year 2699.
* The date/month/year combination is checked for validity after the bus is deasserted. The `date` field is clipped to the next valid date. No such check is done for alarms, they will just fail to trigger.

## Usage example

The `Soft323x<SRAM_SIZE>` object mainly features two functions:

* `tick()` should be called from an ISR and will advance the internal second counter by one.
* `update()` commits the internal second counter to the registers.

### Porting to other platforms

Given a standard compliant C++14 compiler and library, this code is 100% platform independent. However, the code requires an atomic update of the tick counter. On more potent target platforms this is accomplished by using the `<atomic>` header from the C++ standard library, which may not be available for 8-bit µCs. The code contains special handling for AVR microcontrollers where ISRs are temporarily disabled during the update using the AVR libc `<util/atomic.h>`. Please feel free to contribute code for other platforms that cannot use the standard library.

## Unit tests

To run the unit tests, install the `meson` and `ninja` build systems, then run

```sh
git clone https://github.com/astoecke/soft323x
cd soft323x; mkdir build; cd build;
meson .. -Dbuildtype=release
./test_soft323x
```

Note that this may take quite a while depending on your computer, sice the unit tests exhaustively simulate running for several hundred years. The code has 100% coverage and nearly about 93% branch coverage (where most untaken branches correspond to unspecified modes in the alarm subsystem).

## License

This code is licensed under the [AGPLv3](https://www.gnu.org/licenses/agpl-3.0.en.html).
