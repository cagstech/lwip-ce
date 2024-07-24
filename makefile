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

functiontable: $(FUNCTABLE_FILE)

$(FUNCTABLE_FILE):
	@echo "Updating $(FUNCTABLE_FILE)..."
	# Step 1: Find header files and exclude those in EXCLUDE_LIST
	if [ -z "$(EXCLUDE_LIST)" ]; then \
		find "$(HEADER_DIRS)" -name "*.h" > header_files.tmp; \
	else \
		echo "Excluding: $(EXCLUDE_LIST)"; \
		find "$(HEADER_DIRS)" -name "*.h" | grep -v $(EXCLUDE_LIST) > header_files.tmp; \
	fi
	
	# Step 2: Extract function prototypes and get the function names
	cat header_files.tmp | xargs grep -E -o '^[a-zA-Z_][a-zA-Z0-9_]*\s+\*?[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*;' | \
	sed 's/.*\s\+\(\w\+\)\s*(.*/\1/' > extracted_functions.tmp
	
	# Step 3: Update functable.h, avoiding duplicates
	touch $(FUNCTABLE_FILE)
	awk 'BEGIN { while ((getline < "$(FUNCTABLE_FILE)") > 0) { \
		if ($$0 ~ /void \*([a-zA-Z_][a-zA-Z0-9_]*)\(\)/) { \
			match($$0, /void \*([a-zA-Z_][a-zA-Z0-9_]*)\(/, arr); \
			existing[arr[1]] = 1; \
		} \
	} } \
	{ if (!existing[$$1]) print "void *" $$1 "();" }' extracted_functions.tmp > temp_functable.h


	# Step 4: Append new functions to functable.h
	cat temp_functable.h >> $(FUNCTABLE_FILE)
	rm header_files.tmp extracted_functions.tmp temp_functable.h

.PHONY: functiontable
