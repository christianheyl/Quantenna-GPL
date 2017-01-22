#!/bin/sed -f

# Skip anything that is not a struct
/^struct[ \t]/!d
/;[ \t]*$/d

# Add opening brace if not present on the 'struct' line
/{/ 				!{ s/$/ {/ }

:next_struct_line
# Remove opening brace if present in column 1
s/^{//

# Split any single lines into multiple lines
s/; /;\n/g

# Delete macros (lines starting with #)
s/\W*\#.*//

# Delete C++ style comments (starting with //)
s/\/\/.*//

# Delete C style comments (/* ... */)
# - skip this loop unless it contains the start of a C comment (/*)
# - if '/*' is found, join lines until the corresponding '*/'
# - on exiting the loop, delete the joined line
/\/\*/ 				!{ b not_in_comment }
:in_comment
/\*\// 				!{ N; b in_comment }
s/\/\*.*\*\///
:not_in_comment

# Read the next line of the struct
/}/ 				!{n; b next_struct_line}

# Remove special attributes
s/}.*__attribute__.*;/};/

# Finished reading a struct - print
