#!/bin/bash

# Game Launcher Script
# This script automatically detects and runs games from the /games folder
# Falls back to the default snake game if no games are found

GAMES_DIR="/games"
DEFAULT_GAME="/app/snake.sh"
GAME_NAME="${GAME_NAME:-}"  # Optional: specify which game to run

echo "[GAME LAUNCHER] Starting game launcher..."
echo "[GAME LAUNCHER] Checking for games in ${GAMES_DIR}..."

# Function to make a file executable and run it
run_game() {
    local game="$1"
    echo "[GAME LAUNCHER] Selected game: $game"
    
    # Make it executable if not already
    chmod +x "$game" 2>/dev/null
    
    # Determine how to run it based on extension
    case "$game" in
        *.sh)
            echo "[GAME LAUNCHER] Launching shell script: $game"
            exec bash "$game"
            ;;
        *.py)
            echo "[GAME LAUNCHER] Launching Python script: $game"
            exec python3 "$game"
            ;;
        *)
            if [ -x "$game" ]; then
                echo "[GAME LAUNCHER] Launching executable: $game"
                exec "$game"
            else
                echo "[GAME LAUNCHER] ERROR: Cannot execute $game (not executable)"
                return 1
            fi
            ;;
    esac
}

# Function to find all games in the games directory
find_games() {
    local games=()
    
    if [ ! -d "$GAMES_DIR" ]; then
        echo "[GAME LAUNCHER] Games directory not found: ${GAMES_DIR}"
        return 1
    fi
    
    # Find all files (not directories) in /games
    while IFS= read -r -d '' game; do
        games+=("$game")
    done < <(find "$GAMES_DIR" -maxdepth 1 -type f -print0 2>/dev/null)
    
    echo "${games[@]}"
}

# Main logic
if [ -d "$GAMES_DIR" ]; then
    # Get list of available games
    games=($(find_games))
    
    if [ ${#games[@]} -gt 0 ]; then
        echo "[GAME LAUNCHER] Found ${#games[@]} game(s) in ${GAMES_DIR}"
        
        # If GAME_NAME is specified, try to find and run that specific game
        if [ -n "$GAME_NAME" ]; then
            echo "[GAME LAUNCHER] Looking for game: $GAME_NAME"
            
            # Check if it's a full path
            if [ -f "$GAME_NAME" ]; then
                run_game "$GAME_NAME"
                exit 0
            fi
            
            # Check in games directory
            if [ -f "${GAMES_DIR}/${GAME_NAME}" ]; then
                run_game "${GAMES_DIR}/${GAME_NAME}"
                exit 0
            fi
            
            # Try to find by partial match
            for game in "${games[@]}"; do
                if [[ "$(basename "$game")" == *"$GAME_NAME"* ]]; then
                    echo "[GAME LAUNCHER] Found matching game: $game"
                    run_game "$game"
                    exit 0
                fi
            done
            
            echo "[GAME LAUNCHER] ERROR: Game '$GAME_NAME' not found in ${GAMES_DIR}"
            echo "[GAME LAUNCHER] Available games:"
            for game in "${games[@]}"; do
                echo "  - $(basename "$game")"
            done
            exit 1
        fi
        
        # No GAME_NAME specified - list games and run the first one
        echo "[GAME LAUNCHER] Available games:"
        for i in "${!games[@]}"; do
            echo "  [$((i+1))] $(basename "${games[$i]}")"
        done
        
        # Run the first game found
        echo "[GAME LAUNCHER] No GAME_NAME specified, running first game..."
        run_game "${games[0]}"
        exit 0
    else
        echo "[GAME LAUNCHER] No games found in ${GAMES_DIR}"
    fi
else
    echo "[GAME LAUNCHER] Games directory does not exist: ${GAMES_DIR}"
fi

# Fall back to default game
echo "[GAME LAUNCHER] Falling back to default game: ${DEFAULT_GAME}"
if [ -f "$DEFAULT_GAME" ]; then
    echo "[GAME LAUNCHER] Launching default game..."
    exec "$DEFAULT_GAME"
else
    echo "[GAME LAUNCHER] ERROR: Default game not found at ${DEFAULT_GAME}"
    echo "[GAME LAUNCHER] No games available to run!"
    exit 1
fi