#############################################################################
#
# Makefile for automatond on Raspberry Pi
#
# License: MIT 
# Author:  harmon25 
# Date:    2016/03/16 (version 1.0)
#
# Description:
# ------------
# use make all and make install to install
# You can change the install directory by editing the prefix line
#
prefix := /usr/local

SRCDIR := src
BUILDDIR := build
TARGETDIR := bin
SRCEXT := cpp

ARCH=armv6zk
ifeq "$(shell uname -m)" "armv7l"
ARCH=armv7-a
endif

# Detect the Raspberry Pi from cpuinfo
#Count the matches for BCM2708 or BCM2709 in cpuinfo
RPI=$(shell cat /proc/cpuinfo | grep Hardware | grep -c BCM2708)
CCFLAGS=-Ofast -mfpu=vfp -mfloat-abi=hard -march=$(ARCH) -mtune=arm1176jzf-s -std=c++0x
ifneq "${RPI}" "1"
RPI=$(shell cat /proc/cpuinfo | grep Hardware | grep -c BCM2709)
CCFLAGS=-std=c++0x -Ofast -march=$(ARCH) -mtune=cortex-a7 -mfpu=neon -mfloat-abi=hard  
endif

# define all programs
PROGRAMS = automatond
SOURCES = ${PROGRAMS:=.cpp}
LIBS=-lb2 -lcrypto++ -lmosquittopp -lrf24-bcm -lrf24network -lArduiPi_OLED


${PROGRAMS}: ${SOURCES}
	g++ ${CCFLAGS} -Wall $@.cpp -o $@ ${LIBS}

clean:
	rm -rf $(PROGRAMS)

install: all
	test -d $(prefix) || mkdir $(prefix)
	test -d $(prefix)/bin || mkdir $(prefix)/bin
	for prog in $(PROGRAMS); do \
		install -m 0755 $$prog $(prefix)/bin; \
	done

.PHONY: install