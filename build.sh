#!/bin/bash

echo "Building oldnotes..."

mkdir -p build
gcc oldnotes.c -ljack -o build/oldnotes

echo ""
echo "Done. Built app in build directory."

