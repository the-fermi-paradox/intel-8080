#!/usr/bin/awk -f
# This script builds out an enum based on an OpCode table
BEGIN { printf("enum OpCode {\n") }
{
  printf("  ")
  field = ""
  for(i = 2; i <= NF; i++) { 
    if (field) field = field "_" $i;
    else field = $i 
  } 
  printf("%-8s = 0x%s,\n", field, $1)
}
END { printf("}\n") }
