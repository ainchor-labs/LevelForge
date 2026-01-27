# LevelForge

LevelForge is a lean, YAML-driven game engine without a GUI. Define your games through configuration files and focus on gameplay logic rather than engine boilerplate.

## Technologies

- **Framework:** Raylib (graphics, input, audio)
- **2D Physics:** Box2D
- **3D Physics:** Jolt
- **Configuration:** YAML

## Installation

### Linux

1. Clone the repository:
```bash
git clone https://github.com/yourusername/LevelForge.git
cd LevelForge
```

2. Install the `forge` CLI tool:
```bash
./install.sh
source ~/.bashrc
```

This installs `forge` to `/usr/local/bin` and sets up the `LEVELFORGE_HOME` environment variable.

### Dependencies

Ensure you have these installed:
- GCC/G++ (C++17 support)
- CMake
- OpenGL development libraries
- X11 development libraries

On Debian/Ubuntu:
```bash
sudo apt install build-essential cmake libgl1-mesa-dev libx11-dev
```

On Fedora:
```bash
sudo dnf install gcc-c++ cmake mesa-libGL-devel libX11-devel
```

## Quick Start

### Create a New Project

```bash
# Create a 2D game project
forge init my_game 2d

# Or create a 3D game project
forge init my_game 3d
```

### Build and Run

```bash
cd my_game
forge build
forge run
```

### Project Structure

After running `forge init`, your project will have:

```
my_game/
├── config.yaml          # Game configuration
├── main.cpp             # Main game code
└── screens/
    ├── title.yaml       # Title/menu screen
    └── level_1.yaml     # First gameplay level
```

## Walkthrough: 2D Platformer

Let's create a simple 2D platformer from scratch.

### Step 1: Initialize the Project

```bash
forge init platformer 2d
cd platformer
```

### Step 2: Configure Your Game

Edit `config.yaml`:

```yaml
game:
  name: "My Platformer"
  version: "1.0.0"
  starting_screen: "title"

type: 2d

window:
  width: 1280
  height: 720
  title: "My Platformer"
  resizable: true
  fullscreen: false
  vsync: true

performance:
  target_fps: 60
```

### Step 3: Design Your Title Screen

Edit `screens/title.yaml`:

```yaml
screen:
  name: "title"
  type: "menu"

background:
  color: [25, 25, 35]

elements:
  - type: "text"
    id: "game_title"
    content: "My Platformer"
    position: [640, 200]
    font_size: 72
    color: [255, 255, 255]
    anchor: "center"

  - type: "text"
    id: "prompt"
    content: "Press ENTER to Play"
    position: [640, 350]
    font_size: 24
    color: [150, 150, 150]
    anchor: "center"

  - type: "button"
    id: "play_btn"
    content: "Play"
    position: [640, 450]
    size: [200, 50]
    background: [80, 140, 200]
    action: "goto_screen:level_1"
    anchor: "center"

input:
  - key: "ENTER"
    action: "goto_screen:level_1"
  - key: "ESCAPE"
    action: "quit"
```

### Step 4: Create Your First Level

Edit `screens/level_1.yaml`:

```yaml
screen:
  name: "level_1"
  type: "gameplay"
  next_screen: "level_2"

background:
  color: [20, 20, 30]

physics:
  enabled: true
  gravity: [0, 9.8]

camera:
  type: "follow"
  target: "player"
  zoom: 1.0
  smoothing: 0.1

entities:
  # Player
  - type: "player"
    id: "player"
    position: [100, 500]
    size: [32, 48]
    color: [100, 200, 255]

  # Ground
  - type: "platform"
    id: "ground"
    position: [640, 700]
    size: [1280, 40]
    color: [80, 80, 80]
    static: true

  # Floating platforms
  - type: "platform"
    id: "plat_1"
    position: [300, 550]
    size: [150, 20]
    color: [100, 100, 100]
    static: true

  - type: "platform"
    id: "plat_2"
    position: [550, 450]
    size: [150, 20]
    color: [100, 100, 100]
    static: true

  - type: "platform"
    id: "plat_3"
    position: [800, 350]
    size: [150, 20]
    color: [100, 100, 100]
    static: true

  # Goal
  - type: "goal"
    id: "finish"
    position: [1100, 300]
    size: [50, 50]
    color: [255, 215, 0]
    action: "complete_level"

ui:
  - type: "text"
    id: "level_label"
    content: "Level 1"
    position: [20, 20]
    font_size: 24
    color: [255, 255, 255]

input:
  - key: "ESCAPE"
    action: "goto_screen:title"
  - key: "R"
    action: "restart_screen"

objectives:
  - type: "reach_goal"
    target: "finish"
```

### Step 5: Add a Pause Menu

Create `screens/pause.yaml`:

```yaml
screen:
  name: "pause"
  type: "pause"
  overlay: true

background:
  color: [0, 0, 0]
  opacity: 0.7

elements:
  - type: "text"
    id: "pause_title"
    content: "PAUSED"
    position: [640, 250]
    font_size: 48
    color: [255, 255, 255]
    anchor: "center"

  - type: "button"
    id: "resume_btn"
    content: "Resume"
    position: [640, 350]
    size: [200, 50]
    background: [60, 120, 200]
    action: "resume"
    anchor: "center"

  - type: "button"
    id: "quit_btn"
    content: "Main Menu"
    position: [640, 420]
    size: [200, 50]
    background: [100, 100, 100]
    action: "goto_screen:title"
    anchor: "center"

input:
  - key: "ESCAPE"
    action: "resume"
```

### Step 6: Validate and Build

```bash
# Check your screen configurations
forge screens validate

# Build the game
forge build

# Run it
forge run
```

## Walkthrough: 3D Game

### Step 1: Initialize

```bash
forge init my_3d_game 3d
cd my_3d_game
```

### Step 2: Configure for 3D

The `config.yaml` will have `type: 3d` which enables Jolt physics.

### Step 3: Create a 3D Level

Edit `screens/level_1.yaml` for 3D:

```yaml
screen:
  name: "level_1"
  type: "gameplay"

background:
  color: [135, 206, 235]  # Sky blue

physics:
  enabled: true
  gravity: [0, -9.8, 0]  # 3D gravity (y-down)

camera:
  type: "orbit"
  target: "player"
  distance: 10.0
  angle: [45, 0]

entities:
  - type: "player"
    id: "player"
    position: [0, 2, 0]
    size: [1, 2, 1]
    color: [100, 200, 255]

  - type: "floor"
    id: "ground"
    position: [0, 0, 0]
    size: [50, 1, 50]
    color: [80, 160, 80]
    static: true

  - type: "box"
    id: "crate_1"
    position: [5, 1, 3]
    size: [2, 2, 2]
    color: [180, 120, 60]

ui:
  - type: "text"
    id: "instructions"
    content: "WASD to move, Mouse to look"
    position: [20, 20]
    font_size: 20
    color: [255, 255, 255]
```

## CLI Reference

### Commands

| Command | Description |
|---------|-------------|
| `forge init <name> [2d\|3d]` | Create a new project |
| `forge build` | Build the current project |
| `forge run` | Build and run the project |
| `forge clean` | Remove build artifacts |
| `forge screens list` | List all screen files |
| `forge screens validate` | Validate screen YAML files |
| `forge help` | Show help |
| `forge version` | Show version |

### Examples

```bash
# Create and run a 2D game
forge init breakout 2d && cd breakout && forge run

# Validate screens in current project
forge screens validate

# Clean and rebuild
forge clean && forge build
```

## Screen Configuration Reference

### Screen Types

- `menu` - Title screens, main menus
- `gameplay` - Active game levels
- `cutscene` - Story/cinematic screens
- `pause` - Pause overlay (use `overlay: true`)

### Common Sections

```yaml
screen:
  name: "screen_name"      # Must match filename
  type: "gameplay"         # Screen type
  next_screen: "level_2"   # Auto-advance on completion
  overlay: true            # Render over previous screen

background:
  color: [r, g, b]         # RGB 0-255
  image: "path/to/bg.png"  # Optional background image
  opacity: 0.7             # For overlays

elements:                  # UI elements (menus)
  - type: "text"
    id: "unique_id"
    content: "Display text"
    position: [x, y]
    font_size: 24
    color: [255, 255, 255]
    anchor: "center"       # left, center, right

  - type: "button"
    id: "btn_id"
    content: "Click Me"
    position: [x, y]
    size: [width, height]
    background: [r, g, b]
    hover_background: [r, g, b]
    action: "goto_screen:next"

entities:                  # Game objects (gameplay)
  - type: "player"
    id: "player"
    position: [x, y]       # or [x, y, z] for 3D
    size: [w, h]           # or [w, h, d] for 3D

physics:
  enabled: true
  gravity: [0, 9.8]        # 2D: [x, y], 3D: [x, y, z]

camera:
  type: "follow"           # static, follow, orbit, free
  target: "player"
  zoom: 1.0
  smoothing: 0.1

input:
  - key: "ESCAPE"
    action: "goto_screen:pause"
  - key: "R"
    action: "restart_screen"

transitions:
  enter: "fade_in"
  exit: "fade_out"
  duration: 0.3
```

### Actions

| Action | Description |
|--------|-------------|
| `goto_screen:<name>` | Switch to another screen |
| `restart_screen` | Restart current screen |
| `resume` | Resume from pause overlay |
| `complete_level` | Mark level complete, go to next |
| `quit` | Exit the game |

## Project Examples

See the `examples/` directory for complete sample projects:

- `examples/basic_game/` - Simple 2D template with screens

## License

MIT License
