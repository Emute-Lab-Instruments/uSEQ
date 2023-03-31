#!/usr/bin/env sh

# Check if we have exactly two arguments
if [ $# -ne 2 ]; then
  echo "Usage: $0 <input-file> <output-file>"
  exit 1
fi

# Read the input file and remove all Lisp comments
content=$(sed 's/;.*//' "$1")

# Turn the content into a valid C string literal
content=$(echo "$content" | sed 's/\\/\\\\/g' | sed 's/"/\\"/g' | sed 's/\n/\\n/g' | sed 's/^/"/' | sed 's/$/\\n"/')

# Write the result to the output file
echo "const char LispLibrary[] PROGMEM =" > "$2"
echo "$content ;" >> "$2"

# Print a success message
echo "Successfully converted '$1' to '$2'"
