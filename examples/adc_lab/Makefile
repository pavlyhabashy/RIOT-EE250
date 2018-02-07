# name of your application
APPLICATION = adc_lab

# If no BOARD is found in the environment, use this default:
BOARD ?= openmote-cc2538

# Use `jlink` to flash over jtag or `cc2538-bsl` to flash over UART
PROGRAMMER ?= cc2538-bsl

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

FEATURES_REQUIRED = periph_adc periph_gpio
USEMODULE += xtimer

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
