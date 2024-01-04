# ----------------------------
# Makefile Options
# ----------------------------

NAME = DEMO
ICON = icon.png
DESCRIPTION = lwIP Networking Stack

APP_NAME = lwIP
APP_VERSION = 0

CFLAGS = -Wall -Wextra -Oz -Isrc/include
CXXFLAGS = -Wall -Wextra -Oz -Isrc/include

# ----------------------------

include app_tools/makefile
