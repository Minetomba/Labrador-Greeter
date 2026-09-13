#!/bin/bash
# SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
# SPDX-License-Identifier: GPL-3.0-only
echo "Installing login-cli..."
sudo install -Dm755 bin/login-cli /usr/local/bin/login-cli
sudo install -Dm644 src/login-cli@.service /etc/systemd/system/login-cli@.service
echo "Reloading systemd..."
sudo systemctl daemon-reload
echo "Masking the normal tty1 getty..."
sudo systemctl mask getty@tty1.service
sudo systemctl mask agetty@tty1.service
echo "Enabling login-cli on tty1..."
sudo systemctl enable login-cli@tty1.service
echo "login-cli installed!"