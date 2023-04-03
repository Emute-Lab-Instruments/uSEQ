import sys
import re

input_file = sys.argv[1]
output_path = sys.argv[2]

# read input file
with open(input_file, 'r') as f:
    content = f.read()

# remove comments
content = re.sub(r';.*', '', content)

# surround top-level S-expressions with quotes and put them into a list
s_expressions = []
open_parens = 0
start = 0
for i, char in enumerate(content):
    if char == '(':
        if open_parens == 0:
            start = i
        open_parens += 1
    elif char == ')':
        open_parens -= 1
        if open_parens == 0:
            s_expression = content[start:i+1]
            s_expression = s_expression.strip()
            s_expression = '"' + s_expression.replace('\\', '\\\\').replace('\n', '\\n') + '"'
            s_expressions.append(s_expression)

# write output file
with open(output_path, 'w') as f:
    f.write('// LispLibrary.h\n')
    f.write('#pragma once\n')
    f.write('const String PROGMEM LispLibrary[] = {\n')
    for i, s_expression in enumerate(s_expressions):
        f.write(s_expression)
        if i != len(s_expressions) - 1:
            f.write(',\n')
        else:
            f.write('\n')
    f.write('};\n')
    f.write(f'\nconst int LispLibrarySize = {len(s_expressions)};\n')
