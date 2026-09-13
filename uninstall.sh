#!/bin/bash
# SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
# SPDX-License-Identifier: GPL-3.0-only
set -e

echo "Deleting login-cli from systemd..."
sudo systemctl stop login-cli@tty1.service || true
sudo systemctl disable login-cli@tty1.service || true
sudo rm -f /etc/systemd/system/login-cli@.service
echo "Removing executable..."
sudo rm -f /usr/local/bin/login-cli
echo "Reloading systemd..."
sudo systemctl daemon-reload
echo "Restoring tty1 getty..."
sudo systemctl unmask getty@tty1.service
sudo systemctl unmask agetty@tty1.service
sudo systemctl enable getty@tty1.service
echo "login-cli removed!"