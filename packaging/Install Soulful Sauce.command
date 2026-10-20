#!/bin/bash
# Installs Just Feezy Soulful Sauce (VST3 + AU) into your user plug-in folders.

cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

echo "Installing Soulful Sauce..."
rm -rf "$VST3_DIR/Soulful Sauce.vst3" "$AU_DIR/Soulful Sauce.component"
cp -R "Soulful Sauce.vst3" "$VST3_DIR/" || exit 1
cp -R "Soulful Sauce.component" "$AU_DIR/" || exit 1

# The build is not signed with a paid Apple developer account yet, so clear the
# "downloaded from the internet" flag that would make macOS block it.
xattr -dr com.apple.quarantine "$VST3_DIR/Soulful Sauce.vst3" "$AU_DIR/Soulful Sauce.component" 2>/dev/null

# Make macOS notice the new Audio Unit
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Done. Installed to:"
echo "  $VST3_DIR/Soulful Sauce.vst3"
echo "  $AU_DIR/Soulful Sauce.component"
echo ""
echo "In FL Studio: Options > Manage plugins > Find more plugins."
echo "Soulful Sauce appears under Generators (made by Just Feezy)."
echo ""
read -n 1 -s -r -p "Press any key to close this window"
echo ""
