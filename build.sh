#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

JOBS=$(nproc)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# ============================================================================
# Library Builders
# ============================================================================

build_raylib() {
    if [ -f "raylib/lib/libraylib.a" ]; then
        log_info "raylib already built, skipping..."
        return 0
    fi

    log_info "Building raylib..."
    mkdir -p raylib/build && cd raylib/build

    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF
    make -j"$JOBS"

    cd "$PROJECT_DIR"
    mkdir -p raylib/include raylib/lib
    cp raylib/src/{raylib.h,raymath.h,rlgl.h,rcamera.h,rgestures.h} raylib/include/
    cp raylib/build/raylib/libraylib.a raylib/lib/

    log_info "raylib built successfully"
}

build_box2d() {
    if [ -f "box2d/lib64/libbox2d.a" ] || [ -f "box2d/lib/libbox2d.a" ]; then
        log_info "box2d already built, skipping..."
        return 0
    fi

    log_info "Building box2d..."
    mkdir -p box2d/build && cd box2d/build

    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j"$JOBS"

    cd "$PROJECT_DIR"
    if [ -f "box2d/build/src/libbox2d.a" ]; then
        mkdir -p box2d/lib64
        cp box2d/build/src/libbox2d.a box2d/lib64/
    fi

    log_info "box2d built successfully"
}

build_jolt() {
    if [ -f "jolt/Build/Linux_Release/libJolt.a" ]; then
        log_info "jolt already built, skipping..."
        return 0
    fi

    log_info "Building jolt..."
    cd jolt/Build

    cmake -S . -B Linux_Release -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=g++
    make -C Linux_Release -j"$JOBS"

    cd "$PROJECT_DIR"
    log_info "jolt built successfully"
}

build_yaml() {
    if [ -f "yaml/lib/libyaml-cpp.a" ]; then
        log_info "yaml-cpp already built, skipping..."
        return 0
    fi

    log_info "Building yaml-cpp..."
    mkdir -p yaml/build && cd yaml/build

    cmake .. -DCMAKE_BUILD_TYPE=Release \
        -DYAML_CPP_BUILD_TESTS=OFF \
        -DYAML_CPP_BUILD_TOOLS=OFF
    make -j"$JOBS"

    cd "$PROJECT_DIR"
    mkdir -p yaml/lib
    cp yaml/build/libyaml-cpp.a yaml/lib/

    log_info "yaml-cpp built successfully"
}

# ============================================================================
# Game Builders
# ============================================================================

build_game_2d() {
    local source_file="${1:-main.cpp}"
    local output_name="${2:-game}"
    log_info "Building 2D game (raylib + box2d + yaml-cpp) from $source_file..."

    BOX2D_LIB_DIR="box2d/lib64"
    [ ! -d "$BOX2D_LIB_DIR" ] && BOX2D_LIB_DIR="box2d/lib"

    g++ "$source_file" -o "$output_name" \
        -I./raylib/include \
        -I./box2d/include \
        -I./yaml/include \
        -L./raylib/lib \
        -L./"$BOX2D_LIB_DIR" \
        -L./yaml/lib \
        -l:libraylib.a \
        -lbox2d \
        -l:libyaml-cpp.a \
        -lGL -lm -lpthread -ldl -lrt -lX11

    log_info "Game built: ./$output_name"
}

build_game_3d() {
    local source_file="${1:-main.cpp}"
    local output_name="${2:-game}"
    log_info "Building 3D game (raylib + jolt) from $source_file..."

    g++ "$source_file" -o "$output_name" \
        -std=c++17 \
        -O2 -DNDEBUG \
        -DJPH_PROFILE_ENABLED \
        -DJPH_DEBUG_RENDERER \
        -DJPH_OBJECT_STREAM \
        -I./raylib/include \
        -I./jolt \
        -L./raylib/lib \
        -L./jolt/Build/Linux_Release \
        -l:libraylib.a \
        -l:libJolt.a \
        -lGL -lm -lpthread -ldl -lrt -lX11

    log_info "Game built: ./$output_name"
}

build_demo_3d() {
    build_raylib
    build_jolt
    build_game_3d "demo_3d.cpp" "demo_3d"
}

# ============================================================================
# Clean Functions
# ============================================================================

clean_raylib() { rm -rf raylib/build raylib/lib raylib/include; }
clean_box2d()  { rm -rf box2d/build box2d/lib64 box2d/lib; }
clean_jolt()   { rm -rf jolt/Build/Linux_Release jolt/Build/Linux_Debug; }
clean_yaml()   { rm -rf yaml/build yaml/lib; }

clean_2d() {
    log_info "Cleaning 2D build artifacts..."
    clean_raylib
    clean_box2d
    clean_yaml
    rm -f game
    log_info "Clean complete"
}

clean_3d() {
    log_info "Cleaning 3D build artifacts..."
    clean_raylib
    clean_jolt
    rm -f game
    log_info "Clean complete"
}

clean_all() {
    log_info "Cleaning all build artifacts..."
    clean_raylib
    clean_box2d
    clean_jolt
    clean_yaml
    rm -f game
    log_info "Clean complete"
}

# ============================================================================
# Update Functions
# ============================================================================

update_repo() {
    local repo_dir="$1"
    local repo_name="$2"

    if [ ! -d "$repo_dir/.git" ]; then
        log_error "$repo_name is not a git repository"
        return 1
    fi

    log_info "Updating $repo_name..."
    cd "$repo_dir"
    git fetch origin

    LOCAL=$(git rev-parse HEAD)
    REMOTE=$(git rev-parse @{u} 2>/dev/null || echo "")

    if [ -z "$REMOTE" ]; then
        log_warn "$repo_name has no upstream set, pulling from origin/main or master"
        git show-ref --verify --quiet refs/remotes/origin/main && git pull origin main || git pull origin master
    elif [ "$LOCAL" = "$REMOTE" ]; then
        log_info "$repo_name is already up to date"
    else
        log_info "Pulling $(git log HEAD..@{u} --oneline | wc -l) new commit(s)"
        git pull
    fi

    cd "$PROJECT_DIR"
}

update_2d() {
    log_info "Updating 2D dependencies..."
    update_repo "raylib" "raylib" && clean_raylib && build_raylib
    update_repo "box2d" "box2d" && clean_box2d && build_box2d
    build_game_2d
    log_info "Update complete"
}

update_3d() {
    log_info "Updating 3D dependencies..."
    update_repo "raylib" "raylib" && clean_raylib && build_raylib
    update_repo "jolt" "jolt" && clean_jolt && build_jolt
    build_game_3d
    log_info "Update complete"
}

# ============================================================================
# Main Commands
# ============================================================================

# Derive output name from source file (e.g., demo_3d.cpp -> demo_3d)
get_output_name() {
    local source_file="$1"
    basename "$source_file" .cpp
}

build_2d() {
    local source_file="${1:-main.cpp}"
    local output_name
    output_name=$(get_output_name "$source_file")
    build_raylib
    build_box2d
    build_yaml
    build_game_2d "$source_file" "$output_name"
}

build_3d() {
    local source_file="${1:-main.cpp}"
    local output_name
    output_name=$(get_output_name "$source_file")
    build_raylib
    build_jolt
    build_game_3d "$source_file" "$output_name"
}

rebuild_2d() {
    clean_2d
    build_2d
}

rebuild_3d() {
    clean_3d
    build_3d
}

usage() {
    echo "Usage: $0 <mode> [command|file.cpp]"
    echo ""
    echo "Modes:"
    echo "  2d          Build 2D game (raylib + box2d)"
    echo "  3d          Build 3D game (raylib + jolt)"
    echo "  demo        Build 3D demo (tennis target game)"
    echo ""
    echo "Commands (optional):"
    echo "  <file.cpp>  Build specific source file (output name derived from filename)"
    echo "  clean       Remove build artifacts for the mode"
    echo "  rebuild     Clean and rebuild from scratch"
    echo "  update      Pull latest from git repos and rebuild"
    echo ""
    echo "Other:"
    echo "  clean-all   Remove all build artifacts"
    echo "  help        Show this message"
    echo ""
    echo "Examples:"
    echo "  $0 2d                  # Build main.cpp as 2D game"
    echo "  $0 3d                  # Build main.cpp as 3D game"
    echo "  $0 3d demo_3d.cpp      # Build demo_3d.cpp -> ./demo_3d"
    echo "  $0 2d my_game.cpp      # Build my_game.cpp -> ./my_game"
    echo "  $0 demo                # Build 3D tennis demo"
    echo "  $0 2d rebuild          # Clean rebuild 2D"
    echo "  $0 3d update           # Update and rebuild 3D"
}

# ============================================================================
# Entry Point
# ============================================================================

case "${1:-}" in
    2d)
        case "${2:-}" in
            clean)   clean_2d ;;
            rebuild) rebuild_2d ;;
            update)  update_2d ;;
            "")      build_2d ;;
            *.cpp)   build_2d "$2" ;;
            *)       log_error "Unknown command: $2"; usage; exit 1 ;;
        esac
        ;;
    3d)
        case "${2:-}" in
            clean)   clean_3d ;;
            rebuild) rebuild_3d ;;
            update)  update_3d ;;
            "")      build_3d ;;
            *.cpp)   build_3d "$2" ;;
            *)       log_error "Unknown command: $2"; usage; exit 1 ;;
        esac
        ;;
    demo)
        build_demo_3d
        ;;
    clean-all)
        clean_all
        ;;
    help|--help|-h|"")
        usage
        ;;
    *)
        log_error "Unknown mode: $1"
        usage
        exit 1
        ;;
esac
