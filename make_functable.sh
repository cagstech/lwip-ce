#!/bin/bash

# Prerequisites
declare -a search_directories=("src/drivers" "src/include/lwip")
declare -a excluded_files=("api.h" "sockets.h")
prototype_match_pattern="([a-zA-Z_][a-zA-Z0-9_]*\s+\**)\s*([a-zA-Z_][a-zA-Z0-9_]*\s*)\(([^)]*)\);"
prototype_no_return_type_pattern="([a-zA-Z_][a-zA-Z0-9_]*\s*)\("
prototype_no_parenthesis_pattern="[a-zA-Z_][a-zA-Z0-9_]*\s*"
output_file=src/functable.inc


# Prepare a pattern to exclude files
exclude_pattern=""
if [ ${#excluded_files[@]} -gt 0 ]; then
    exclude_pattern=$(printf "|%s" "${excluded_files[@]}")
    exclude_pattern="$exclude_pattern"  # Exclude the script itself
    exclude_pattern=${exclude_pattern:1}  # Remove the initial |
fi

[[ ! -f $output_file ]] && touch $output_file

# Function to process files
process_file() {
    local file=$1
    echo "Processing file: $file"
    local prefix=$(basename $file .h)
    /usr/local/bin/ctags -f- --kinds-C=p $file | awk '{ print $1; }' | sed 's/$/,/' | while read -r line; do
        if ! grep -Fxq "$line" "$output_file"; then
            echo "appending function: $line"
            echo "$line" >> "$output_file"
        fi
    done
}


# Logic
for dir in "${search_directories[@]}"; do
    # Find all C header files that are not in the excluded list
    if [[ -n "$exclude_pattern" ]]; then
        find "$dir" -maxdepth 1 -type f -name "*.h" | grep -Ev "$exclude_pattern" | while read -r file; do
            process_file "$file"
        done
    else
        find "$dir" -maxdepth 1 -type f -name "*.h" | while read -r file; do
            process_file "$file"
        done
    fi
done

