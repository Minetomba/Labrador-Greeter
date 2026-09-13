#!/bin/bash
# SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
# SPDX-License-Identifier: GPL-3.0-only
echo "Compiling labrador-greeter..."
gcc -Wall -Wextra -Wpedantic -O2 src/labrador-greeter.c -o bin/labrador-greeter -lpam -lpam_misc
echo "Stripping..."
strip bin/labrador-greeter
echo "Compilation complete."