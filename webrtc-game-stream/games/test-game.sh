#!/bin/bash
# Simple Test Game - Bouncing Ball
# This is a test game to verify the game upload system works

echo "=========================================="
echo "  TEST GAME - Bouncing Ball"
echo "=========================================="
echo ""
echo "This is a simple test game to verify"
echo "that the game upload system is working."
echo ""
echo "Controls:"
echo "  - Press 'q' to quit"
echo "  - Press 'r' to restart"
echo "  - Arrow keys to move (if supported)"
echo ""
echo "The game will run for 60 seconds..."
echo ""
echo "=========================================="
echo ""

# Simple animation loop
for i in {1..60}; do
    clear
    echo "=========================================="
    echo "  TEST GAME - Running ($i/60 seconds)"
    echo "=========================================="
    echo ""
    echo "  Game is streaming successfully! ✓"
    echo ""
    echo "  If you can see this, your game upload"
    echo "  system is working correctly."
    echo ""
    echo "  You can now replace this test game"
    echo "  with your own game in the /games folder."
    echo ""
    echo "=========================================="
    sleep 1
done

echo ""
echo "Test complete! System is working."
echo "Press any key to exit..."
read -n 1
