# ----------------------------
# Makefile Options
# ----------------------------

NAME = ECMDRVCE
ICON = icon.png
DESCRIPTION = "ECM driver test"
COMPRESSED = NO
ARCHIVED = NO
HAS_PRINTF := YES
LTO = NO


CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

ifndef CEDEV
$(error CEDEV environment path variable is not set)
endif

include $(CEDEV)/meta/makefile.mk
