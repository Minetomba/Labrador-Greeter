#!/bin/bash
# SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
# SPDX-License-Identifier: GPL-3.0-only
echo "Compiling login-cli..."
gcc -Wall -Wextra -Wpedantic -O2 src/login-cli.c -o bin/login-cli -lpam -lpam_misc
echo "Stripping..."
strip bin/login-cli
echo "Compilation complete."