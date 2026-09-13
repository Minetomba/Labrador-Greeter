#!/bin/bash
# SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
# SPDX-License-Identifier: GPL-3.0-only
echo "Installing labrador-greeter..."
sudo install -Dm755 bin/labrador-greeter /usr/local/bin/labrador-greeter
sudo install -Dm644 src/labrador-greeter@.service /etc/systemd/system/labrador-greeter@.service
echo "Reloading systemd..."
sudo systemctl daemon-reload
echo "Masking the normal tty1 getty..."
sudo systemctl mask getty@tty1.service
sudo systemctl mask agetty@tty1.service
echo "Enabling labrador-greeter on tty1..."
sudo systemctl enable labrador-greeter@tty1.service
echo "labrador-greeter installed!"