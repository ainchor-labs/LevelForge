#!/bin/bash
# LevelForge Install Script
# Installs the forge CLI tool to /usr/local/bin

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="/usr/local/bin"
FORGE_SOURCE="$SCRIPT_DIR/forge"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Installing LevelForge CLI..."

if [ ! -f "$FORGE_SOURCE" ]; then
    echo -e "${RED}Error:${NC} forge not found at $FORGE_SOURCE"
    exit 1
fi

# Install forge to /usr/local/bin
if [ -w "$INSTALL_DIR" ]; then
    cp "$FORGE_SOURCE" "$INSTALL_DIR/forge"
    chmod +x "$INSTALL_DIR/forge"
else
    echo "Requires sudo to install to $INSTALL_DIR"
    sudo cp "$FORGE_SOURCE" "$INSTALL_DIR/forge"
    sudo chmod +x "$INSTALL_DIR/forge"
fi

# Set LEVELFORGE_HOME if not already set
if ! grep -q "LEVELFORGE_HOME" ~/.bashrc 2>/dev/null; then
    echo "" >> ~/.bashrc
    echo "# LevelForge engine path" >> ~/.bashrc
    echo "export LEVELFORGE_HOME=\"$SCRIPT_DIR\"" >> ~/.bashrc
    echo -e "${GREEN}Added LEVELFORGE_HOME to ~/.bashrc${NC}"
fi

echo -e "${GREEN}Installed:${NC} $INSTALL_DIR/forge"
echo -e "${GREEN}Engine:${NC} $SCRIPT_DIR"
echo ""
echo "Run 'source ~/.bashrc' or start a new terminal to use forge."
