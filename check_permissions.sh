#!/bin/bash

# This script takes a file path as an argument and prints its permissions
file_path=$1

if [ -f "$file_path" ] || [ -d "$file_path" ]; then
    stat -c "%a" "$file_path"
else
    echo "0"
fi
