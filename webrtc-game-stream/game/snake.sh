#!/usr/bin/env bash
# Snake in Bash - terminal version
# Controls: Arrow keys or W/A/S/D (H/J/K/L). Pause 'p', Quit 'q'.

set -u

# ---------- Terminal helpers ----------
cursor_off() { tput civis 2>/dev/null || printf '\e[?25l'; }
cursor_on()  { tput cnorm 2>/dev/null || printf '\e[?25h'; }
move()       { printf '\e[%d;%dH' "$1" "$2"; }   # move row col
clear_scr()  { printf '\e[2J'; }
color()      { tput setaf "$1" 2>/dev/null || true; }
bold()       { tput bold 2>/dev/null || true; }
reset_fmt()  { tput sgr0 2>/dev/null || printf '\e[0m'; }

restore_terminal() {
  stty echo -icanon time 0 min 0 2>/dev/null
  cursor_on
  reset_fmt
}

die() {
  restore_terminal
  move "$STATUS_LINE" 1
  reset_fmt
  echo
  exit "${1:-0}"
}

trap 'die 0' EXIT INT TERM

# ---------- Layout ----------
COLS=$(tput cols)
LINES=$(tput lines)

# Border rectangle; status at last line
LEFT=1
TOP=1
RIGHT=$COLS
BOTTOM=$((LINES - 1))     # bottom border line
STATUS_LINE=$LINES

PLAY_W=$((RIGHT - LEFT - 1))
PLAY_H=$((BOTTOM - TOP - 1))

if (( PLAY_W < 20 || PLAY_H < 10 )); then
  echo "Terminal too small. Please resize to at least ~22x12."
  exit 1
fi

# ---------- Game state ----------
# Snake body stored as parallel arrays of coordinates (head is index 0)
declare -a SX=()
declare -a SY=()
SNAKE_LEN=3

# Start position roughly centered
HEAD_X=$(( LEFT + 1 + PLAY_W / 2 ))
HEAD_Y=$(( TOP + 1 + PLAY_H / 2 ))
DIR="R"   # U/D/L/R

# Initialize snake horizontal to the left
SX=("$HEAD_X" $((HEAD_X-1)) $((HEAD_X-2)))
SY=("$HEAD_Y" "$HEAD_Y" "$HEAD_Y")

# Food
FOOD_X=0
FOOD_Y=0

SCORE=0
PAUSED=0
GAME_OVER=0

# Speed tiers (strings for read -t)
SPEED=( "0.12" "0.11" "0.10" "0.09" "0.08" "0.07" "0.06" "0.055" "0.05" "0.045" "0.04" "0.035" "0.03" )

tick_time() {
  local tier=$(( SCORE / 50 ))
  (( tier >= ${#SPEED[@]} )) && tier=$((${#SPEED[@]} - 1))
  echo "${SPEED[$tier]}"
}

# ---------- Drawing ----------
draw_border() {
  # Top
  move "$TOP" "$LEFT";       printf '+'
  for ((x=LEFT+1; x<RIGHT; x++)); do printf '-'; done
  printf '+'

  # Sides
  for ((y=TOP+1; y<BOTTOM; y++)); do
    move "$y" "$LEFT";  printf '|'
    move "$y" "$RIGHT"; printf '|'
  done

  # Bottom
  move "$BOTTOM" "$LEFT";    printf '+'
  for ((x=LEFT+1; x<RIGHT; x++)); do printf '-'; done
  printf '+'
}

status_line() {
  move "$STATUS_LINE" 1
  reset_fmt
  printf " Score: "
  bold; printf "%d" "$SCORE"; reset_fmt
  printf "    Controls: ↑↓←→ / WASD (HJKL)   Pause: P   Quit: Q  "
  if (( PAUSED )); then
    bold; color 3; printf "   [PAUSED]"; reset_fmt
  fi
  # Clear remainder of line
  local rem=$(( COLS - 1 - $(tput cols 2>/dev/null || echo 0) ))
  printf '\r'
}

draw_cell() {
  local x=$1 y=$2 ch=$3
  move "$y" "$x"
  printf "%s" "$ch"
}

is_on_snake() {
  local x=$1 y=$2
  local i
  for ((i=0; i<${#SX[@]}; i++)); do
    if (( SX[i] == x && SY[i] == y )); then
      return 0
    fi
  done
  return 1
}

place_food() {
  local x y
  while :; do
    x=$(( LEFT + 1 + (RANDOM % PLAY_W) ))
    y=$(( TOP  + 1 + (RANDOM % PLAY_H) ))
    # Ensure not on snake
    if ! is_on_snake "$x" "$y"; then
      FOOD_X=$x; FOOD_Y=$y
      break
    fi
  done
  color 2
  draw_cell "$FOOD_X" "$FOOD_Y" "@"
  reset_fmt
}

draw_snake_initial() {
  color 6
  # Head
  draw_cell "${SX[0]}" "${SY[0]}" "O"
  # Body
  for ((i=1; i<${#SX[@]}; i++)); do
    draw_cell "${SX[i]}" "${SY[i]}" "o"
  done
  reset_fmt
}

update_status() { status_line; }

# ---------- Input ----------
# Reads a direction if available; returns via global NEW_DIR (U/D/L/R or empty)
read_input() {
  NEW_DIR=""
  local key k1 k2
  IFS= read -rsn1 -t "$(tick_time)" key || return 0

  case "$key" in
    $'\e') # Escape sequence (arrow keys)
      IFS= read -rsn1 -t 0.02 k1 || return 0
      if [[ "$k1" == "[" ]]; then
        IFS= read -rsn1 -t 0.02 k2 || return 0
        case "$k2" in
          A) NEW_DIR="U" ;;
          B) NEW_DIR="D" ;;
          C) NEW_DIR="R" ;;
          D) NEW_DIR="L" ;;
        esac
      fi
      ;;
    [WwKk]) NEW_DIR="U" ;;
    [SsJj]) NEW_DIR="D" ;;
    [DdLl]) NEW_DIR="R" ;;
    [AaHh]) NEW_DIR="L" ;;
    [Pp])   PAUSED=$((1-PAUSED));;
    [Qq])   GAME_OVER=1 ;;
  esac
}

# Opposite direction check
is_opposite() {
  local a=$1 b=$2
  [[ ( "$a" == "U" && "$b" == "D" ) || ( "$a" == "D" && "$b" == "U" ) || \
     ( "$a" == "L" && "$b" == "R" ) || ( "$a" == "R" && "$b" == "L" ) ]]
}

# ---------- Movement & game loop ----------
step() {
  local dx=0 dy=0
  case "$DIR" in
    U) dy=-1 ;;
    D) dy=+1 ;;
    L) dx=-1 ;;
    R) dx=+1 ;;
  esac

  local nx=$(( SX[0] + dx ))
  local ny=$(( SY[0] + dy ))

  # Collision with walls?
  if (( nx <= LEFT || nx >= RIGHT || ny <= TOP || ny >= BOTTOM )); then
    GAME_OVER=1; return
  fi
  # Collision with self?
  if is_on_snake "$nx" "$ny"; then
    GAME_OVER=1; return
  fi

  # Move: add new head
  SX=("$nx" "${SX[@]}")
  SY=("$ny" "${SY[@]}")

  # Draw head
  color 6
  draw_cell "$nx" "$ny" "O"
  # Old head becomes body
  if ((${#SX[@]} > 1)); then
    draw_cell "${SX[1]}" "${SY[1]}" "o"
  fi
  reset_fmt

  # Check food
  if (( nx == FOOD_X && ny == FOOD_Y )); then
    SCORE=$((SCORE + 10))
    SNAKE_LEN=$((SNAKE_LEN + 1))
    place_food
    update_status
  else
    # Remove tail (erase)
    local tail_i=$((${#SX[@]} - 1))
    local tx=${SX[$tail_i]}
    local ty=${SY[$tail_i]}
    unset 'SX[tail_i]'
    unset 'SY[tail_i]'
    draw_cell "$tx" "$ty" " "
  fi
}

game_over_screen() {
  color 1; bold
  local msg=" GAME OVER "
  local y=$(( TOP + PLAY_H/2 ))
  local x=$(( LEFT + (PLAY_W/2) - (${#msg}/2) + 1 ))
  move "$y" "$x"; printf "%s" "$msg"
  reset_fmt
  move "$((y+1))" "$((x-8))"
  printf " Final Score: %d   Press any key to exit " "$SCORE"
  # Wait for a key
  stty -echo -icanon time 0 min 1 2>/dev/null
  dd bs=1 count=1 status=none 2>/dev/null
}

# ---------- Start ----------
clear_scr
cursor_off
stty -echo -icanon time 0 min 0 2>/dev/null

draw_border
status_line
draw_snake_initial
place_food

# Main loop
while (( ! GAME_OVER )); do
  read_input

  if (( GAME_OVER )); then
    break
  fi

  # Direction update (avoid reversing into yourself)
  if [[ -n "${NEW_DIR}" ]] && ! is_opposite "$DIR" "$NEW_DIR"; then
    DIR="$NEW_DIR"
  fi

  if (( PAUSED )); then
    # While paused, block softly for a key and only toggle with P/Q
    IFS= read -rsn1 -t 0.1 key && {
      case "$key" in
        [Pp]) PAUSED=0 ;;
        [Qq]) GAME_OVER=1 ;;
      esac
    }
    update_status
    continue
  fi

  step
done

game_over_screen
die 0
