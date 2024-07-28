# ----------------------------
# Makefile Options
# ----------------------------

NAME = lwIP
ICON = icon.png
DESCRIPTION = lwIP Networking Stack DEMO

APP_NAME = lwIP
APP_VERSION = 0

CFLAGS = -Wall -Wextra -Oz -I src/include
CXXFLAGS = -Wall -Wextra -Oz -Isrc/include
BSSHEAP_LOW ?= D0BFD8
BSSHEAP_HIGH ?= D13FD8
OUTPUT_MAP = YES

# ----------------------------

include app_tools/makefile
