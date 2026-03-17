# Games Folder

This folder is where you upload your games to be streamed via WebRTC.

## How to Use

1. **Upload Your Game**: Place your game executable or script in this folder
2. **Make it Executable**: Ensure your game file has execute permissions
3. **Select Your Game** (Optional): Specify which game to run using the `GAME_NAME` environment variable
4. **Restart the Container**: The game container will automatically detect and run your game

### Method 1: Run First Game (Default)

If you don't specify a game name, the launcher will automatically run the first game it finds:

```bash
# Just place your game in the folder
cp my-game.sh games/
chmod +x games/my-game.sh

# Restart to run it
docker-compose restart game
```

### Method 2: Select Specific Game

To run a specific game when you have multiple games, set the `GAME_NAME` environment variable in `docker-compose.yml`:

```yaml
game:
  environment:
    - GAME_NAME=my-game.sh
```

Or use it directly when starting:

```bash
# Run a specific game
GAME_NAME=my-game.sh docker-compose up game

# Or restart with a specific game
docker-compose stop game
GAME_NAME=my-game.sh docker-compose up -d game
```

The `GAME_NAME` can be:
- Full filename: `GAME_NAME=my-game.sh`
- Partial match: `GAME_NAME=my-game` (will match `my-game.sh`)
- Full path: `GAME_NAME=/games/my-game.sh`

## Supported Game Types

The game launcher automatically detects and runs:

- **Shell Scripts** (`.sh`): Executed with `bash`
- **Python Scripts** (`.py`): Executed with `python3`
- **Binary Executables**: Any executable file with execute permissions
- **Other Scripts**: Any file with execute permissions

## File Requirements

### For Shell Scripts (`.sh`)
```bash
#!/bin/bash
# Your game code here
```

### For Python Scripts (`.py`)
```python
#!/usr/bin/env python3
# Your game code here
```

### For Binary Executables
- Must have execute permissions (`chmod +x your-game`)
- Must be compatible with the container's Linux environment (Ubuntu 22.04)

## Examples

### Example 1: Simple Shell Script Game
Create a file `my-game.sh`:
```bash
#!/bin/bash
echo "Starting my game..."
# Your game logic here
```

Make it executable:
```bash
chmod +x games/my-game.sh
```

### Example 2: Python Game
Create a file `my-game.py`:
```python
#!/usr/bin/env python3
import pygame
# Your pygame code here
```

### Example 3: Compiled Binary
```bash
# Copy your compiled game
cp /path/to/your-game games/my-game
chmod +x games/my-game
```

## Game Detection Priority

The launcher checks for games in this order:
1. First executable file found in `/games` directory
2. Falls back to the default snake game if no games are found

## Important Notes

- **Only ONE game runs at a time**: The launcher will run the first game it finds
- **File Permissions**: Make sure your game has execute permissions
- **Dependencies**: Ensure your game's dependencies are installed in the container
- **Display**: Games run in Xvfb virtual display (`:99`)
- **Input**: Keyboard and mouse input is captured from the web browser

## Troubleshooting

### Game Not Starting
1. Check file permissions: `ls -la games/`
2. Verify the file is executable: `chmod +x games/your-game`
3. Check container logs: `docker-compose logs game`

### Game Crashes
1. Check if dependencies are installed in the container
2. Review the game container logs for error messages
3. Ensure the game is compatible with X11/Xvfb

### No Input Response
- The game must be an X11 application to receive input
- Terminal-based games work best with `xterm`

## Adding Dependencies

If your game requires additional packages, modify `game/Dockerfile`:

```dockerfile
RUN apt-get update && apt-get install -y \
    your-package-here \
    && rm -rf /var/lib/apt/lists/*
```

Then rebuild: `docker-compose build game`

## Default Game

If no games are found in this folder, the system falls back to the default snake game (`/app/snake.sh`).
