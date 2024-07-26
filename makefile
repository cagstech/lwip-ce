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


# defining a build rule for the generation of a function table
# to ensure all modules are built into lwip
HEADER_DIRS := src/include/lwip/
EXCLUDE_LIST := src/include/lwip/debug.h
FUNCTABLE_FILE := src/functable.h
HELPER_FILES := $(FUNCTABLE_FILE) tmp/headers.tmp

functiontable: $(HELPER_FILES)

$(HELPER_FILES):
	./make_functable.sh
	

.PHONY: functiontable
