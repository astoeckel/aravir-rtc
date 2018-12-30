#  soft323x -- Software implementation of the DS323x RTC for 8-bit µCs
#  Copyright (C) 2019  Andreas Stöckel
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as
#  published by the Free Software Foundation, either version 3 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.

project('soft323x', 'cpp', default_options : ['cpp_std=c++14'])

# Include directory
inc_soft323x = include_directories('.')

# Compile and register the unit tests
dep_foxenunit = dependency(
    'libfoxenunit',
    fallback:['libfoxenunit', 'dep_foxenunit'])
exe_test_soft323x = executable(
    'test_soft323x',
    'test/test_soft323x.cpp',
    include_directories: inc_soft323x,
    dependencies: dep_foxenunit,
    install: false)
test('test_soft323x', exe_test_soft323x)

# Install the header file
install_headers(
    ['soft323x/soft323x.hpp'],
    subdir: 'foxen')

# Generate a Pkg config file
pkg = import('pkgconfig')
pkg.generate(
    subdirs: [],
    name: 'soft323x',
    version: '1.0',
    filebase: 'soft323x',
    description: 'Software implementation of the DS323x RTC for 8-bit µCs')

# Export the dependency
dep_soft323x = declare_dependency(
    include_directories: inc_soft323x)
