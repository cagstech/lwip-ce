# ----------------------------
# Makefile Options
# ----------------------------

NAME = lwIP
ICON = icon.png
DESCRIPTION = lwIP Networking Stack

APP_NAME = lwIP
APP_VERSION = 0

CFLAGS = -Wall -Wextra -Oz -I src/include -I mbedtls/include
CXXFLAGS = -Wall -Wextra -Oz -Isrc/include -Imbedtls/include
EXTRA_C_SOURCES = mbedtls/library/*.c
OUTPUT_MAP = YES

# ----------------------------

include app_tools/makefile
