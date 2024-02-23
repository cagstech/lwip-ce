# ----------------------------
# Makefile Options
# ----------------------------

NAME = DEMO
ICON = icon.png
DESCRIPTION = lwIP Networking Stack

APP_NAME = lwIP
APP_VERSION = 0

CFLAGS = -Wall -Wextra -Oz -Isrc/include -Imbedtls/include
CXXFLAGS = -Wall -Wextra -Oz -Isrc/include -Imbedtls/include
OUTPUT_MAP = YES

# ----------------------------

include app_tools/makefile
