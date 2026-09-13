#!/bin/bash
# SPDX-FileCopyrightText: 2026 Minetomba <minetomba@proton.me>
# SPDX-License-Identifier: GPL-3.0-only
set -e

echo "Deleting labrador-greeter from systemd..."
sudo systemctl stop labrador-greeter@tty1.service || true
sudo systemctl disable labrador-greeter@tty1.service || true
sudo rm -f /etc/systemd/system/labrador-greeter@.service
echo "Removing executable..."
sudo rm -f /usr/local/bin/labrador-greeter
echo "Reloading systemd..."
sudo systemctl daemon-reload
echo "Restoring tty1 getty..."
sudo systemctl unmask getty@tty1.service
sudo systemctl unmask agetty@tty1.service
sudo systemctl enable getty@tty1.service
echo "labrador-greeter removed!"