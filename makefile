# ----------------------------
# Makefile Options
# ----------------------------

NAME = lwIPDEMO
ICON = icon.png
DESCRIPTION = lwIP Networking Stack DEMO

APP_NAME = lwIPDEMO
APP_VERSION = 0

CFLAGS = -Wall -Wextra -Oz -I src/include
CXXFLAGS = -Wall -Wextra -Oz -Isrc/include
OUTPUT_MAP = YES

# ----------------------------

include app_tools/makefile
