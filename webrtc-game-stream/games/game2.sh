#!/bin/bash
# Second Test Game - Color Demo
# This demonstrates multiple game selection

echo "=========================================="
echo "  GAME 2 - Color Demo"
echo "=========================================="
echo ""
echo "This is the SECOND test game."
echo ""
echo "If you see this, game selection is working!"
echo ""
echo "To run this game specifically, set:"
echo "  GAME_NAME=game2.sh"
echo ""
echo "=========================================="
echo ""

# Color animation loop
colors=("Red" "Green" "Blue" "Yellow" "Magenta" "Cyan")
for i in {1..30}; do
    clear
    color=${colors[$((i % 6))]}
    echo "=========================================="
    echo "  GAME 2 - Color Demo ($i/30)"
    echo "=========================================="
    echo ""
    echo "  Current Color: $color"
    echo ""
    echo "  This is game #2 in your games folder."
    echo ""
    echo "  Game selection is working correctly! ✓"
    echo ""
    echo "=========================================="
    sleep 1
done

echo ""
echo "Game 2 complete!"
read -n 1
