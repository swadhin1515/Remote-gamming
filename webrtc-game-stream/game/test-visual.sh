#!/usr/bin/env bash
# Simple visual test that draws colored boxes moving around the screen

export DISPLAY=:1

while true; do
  clear
  
  # Get random position
  x=$((RANDOM % 60 + 10))
  y=$((RANDOM % 20 + 5))
  
  # Move cursor and draw colored box
  tput cup $y $x
  
  # Random color (1-7)
  color=$((RANDOM % 7 + 1))
  tput setaf $color
  tput bold
  
  echo "████████████"
  echo "████████████"
  echo "████████████"
  echo "████████████"
  
  tput sgr0
  
  # Show some text
  tput cup 2 2
  echo "WebRTC Game Streaming Test"
  tput cup 3 2
  echo "Time: $(date +%H:%M:%S)"
  tput cup 4 2
  echo "If you see this moving, streaming works!"
  
  sleep 0.5
done
