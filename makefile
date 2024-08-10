# ----------------------------
# Makefile Options
# ----------------------------

NAME = lwIP
ICON = icon.png
DESCRIPTION = lwIP Networking Stack

APP_NAME = lwIP
APP_VERSION = 0

CFLAGS = -Wall -Wextra -Oz -I src/include
CXXFLAGS = -Wall -Wextra -Oz -Isrc/include
OUTPUT_MAP = YES

BSSHEAP_LOW ?= D052C6
# BSSHEAP_LOW ?= D11FD8
# BSSHEAP_HIGH ?= D13FD8
# ----------------------------

include app_tools/makefile
