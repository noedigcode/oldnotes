#!/bin/bash

echo "Building oldnotes..."

mkdir -p build
cd build
gcc ../oldnotes.c -ljack -o oldnotes

echo ""
echo "Done. Built app in build directory."

