#!/usr/bin/env bash
# setup-dev.sh — Local Dittobytes development environment setup
#
# Run from the root of your dittobytes checkout:
#   chmod +x setup-dev.sh && ./setup-dev.sh
#
# After setup, open the project in Zed and clangd will provide
# completions for LLVM internals, target-specific headers, etc.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="$PROJECT_ROOT/deps"
LLVM_SOURCE="$DEPS_DIR/llvm-source"
LLVM_BUILD="$DEPS_DIR/llvm-source/build"
LLVM_INSTALL="$DEPS_DIR/llvm-install"
LLVM_WINLIN="$DEPS_DIR/llvm-winlin"
MACOS_SDK="$DEPS_DIR/macos-sdk"

NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "=== Dittobytes local dev setup ==="
echo "Project root: $PROJECT_ROOT"
echo "Parallel jobs: $NPROC"
echo ""

mkdir -p "$DEPS_DIR"

# ── 1. macOS SDK ──
if [ ! -d "$MACOS_SDK" ]; then
    echo "[1/5] Cloning macOS SDK..."
    git clone --depth 1 https://github.com/tijme/forked-dittobytes-macos-sdk.git "$MACOS_SDK"
else
    echo "[1/5] macOS SDK already present, skipping."
fi

# ── 2. Pre-built llvm-mingw (for cross-compilation) ──
if [ ! -d "$LLVM_WINLIN" ]; then
    echo "[2/5] Downloading llvm-mingw toolchain..."
    ARCH=$(arch)
    TARBALL="llvm-mingw-20240619-ucrt-ubuntu-20.04-${ARCH}.tar.xz"
    wget -q --show-progress -P "$DEPS_DIR" \
        "https://github.com/mstorsjo/llvm-mingw/releases/download/20240619/${TARBALL}"
    tar -xf "$DEPS_DIR/$TARBALL" -C "$DEPS_DIR"
    mv "$DEPS_DIR/llvm-mingw-20240619-ucrt-ubuntu-20.04-${ARCH}" "$LLVM_WINLIN"
    rm "$DEPS_DIR/$TARBALL"
else
    echo "[2/5] llvm-mingw already present, skipping."
fi

# ── 3. Clone forked LLVM ──
if [ ! -d "$LLVM_SOURCE" ]; then
    echo "[3/5] Cloning forked Dittobytes LLVM (shallow)..."
    git clone --depth 1 --branch release/18.x \
        https://github.com/tijme/forked-dittobytes-llvm-project.git "$LLVM_SOURCE"
else
    echo "[3/5] LLVM source already present, skipping."
fi

# ── 4. Build LLVM ──
if [ ! -f "$LLVM_INSTALL/bin/clang" ]; then
    echo "[4/5] Building LLVM (this will take a while — ~2.5 hrs first time)..."
    mkdir -p "$LLVM_BUILD"

    cmake -G Ninja -S "$LLVM_SOURCE/llvm" -B "$LLVM_BUILD" \
        -DLLVM_ENABLE_PROJECTS="clang;lld" \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_TARGETS_TO_BUILD="X86;AArch64" \
        -DCMAKE_INSTALL_PREFIX="$LLVM_INSTALL" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DBUILD_SHARED_LIBS=On

    ninja -C "$LLVM_BUILD" -j "$NPROC"
    ninja -C "$LLVM_BUILD" install

    # Copy target-specific headers (needed for in-tree machine passes)
    cp -r "$LLVM_BUILD/lib/Target/X86/" "$LLVM_INSTALL/include/llvm/Target/"
    cp -r "$LLVM_SOURCE/llvm/lib/Target/X86/" "$LLVM_INSTALL/include/llvm/Target/"
    cp -r "$LLVM_BUILD/lib/Target/AArch64/" "$LLVM_INSTALL/include/llvm/Target/"
    cp -r "$LLVM_SOURCE/llvm/lib/Target/AArch64/" "$LLVM_INSTALL/include/llvm/Target/"
else
    echo "[4/5] LLVM already built and installed, skipping."
fi

# ── 5. Configure clangd for Zed ──
echo "[5/5] Configuring clangd for Zed..."

# The compile_commands.json lives in the LLVM build dir. clangd needs
# to find it when editing files anywhere in the LLVM source tree.
# We place a .clangd config at the LLVM source root so it covers
# all pass source files underneath.
cat > "$LLVM_SOURCE/.clangd" << EOF
# yaml-language-server: \$schema=https://json.schemastore.org/clangd.json
CompileFlags:
  CompilationDatabase: build/
EOF

# Also symlink compile_commands.json to the project root for convenience.
# This helps if you open the dittobytes root in Zed and edit files that
# reference LLVM headers (e.g. transpiler wrappers outside the LLVM tree).
ln -sf "$LLVM_BUILD/compile_commands.json" "$PROJECT_ROOT/compile_commands.json"

# Create a .clangd at the project root too, pointing at the LLVM build db.
# This is a fallback for files outside the LLVM source tree.
cat > "$PROJECT_ROOT/.clangd" << EOF
# yaml-language-server: \$schema=https://json.schemastore.org/clangd.json
CompileFlags:
  CompilationDatabase: deps/llvm-source/build/
EOF

# Create Zed project-local settings for clangd
mkdir -p "$PROJECT_ROOT/.zed"
cat > "$PROJECT_ROOT/.zed/settings.json" << 'EOF'
{
  "lsp": {
    "clangd": {
      "binary": {
        "arguments": [
          "--background-index",
          "--clang-tidy=false",
          "--header-insertion=never",
          "--all-scopes-completion",
          "--completion-style=detailed"
        ]
      }
    }
  }
}
EOF

# Create Zed tasks for building and testing
cat > "$PROJECT_ROOT/.zed/tasks.json" << EOF
[
  {
    "label": "LLVM: rebuild (incremental)",
    "command": "ninja -C deps/llvm-source/build -j $NPROC llc clang lld && ninja -C deps/llvm-source/build install",
    "use_new_terminal": false
  },
  {
    "label": "Dittobytes: build beacon (win-amd64-raw)",
    "command": "make beacon-win-amd64-raw",
    "use_new_terminal": false
  }
]
EOF

echo ""
echo "=== Setup complete ==="
echo ""
echo "Add this to your shell profile (.bashrc, .zshrc, etc.):"
echo ""
echo "  export PATH=\"$LLVM_INSTALL/bin:$LLVM_WINLIN/bin:\$PATH\""
echo ""
echo "Or for this session only:"
echo ""
echo "  source <(echo 'export PATH=\"$LLVM_INSTALL/bin:$LLVM_WINLIN/bin:\$PATH\"')"
echo ""
echo "Then open the project in Zed. clangd will index in the background"
echo "and provide completions for LLVM internals."
echo ""
echo "Dev loop:"
echo "  1. Edit pass sources in deps/llvm-source/llvm/lib/..."
echo "  2. Ctrl+Shift+P → task: spawn → 'LLVM: rebuild (incremental)'"
echo "  3. Ctrl+Shift+P → task: spawn → 'Dittobytes: build beacon (win-amd64-raw)'"
echo ""
echo "Or from the terminal:"
echo "  ninja -C deps/llvm-source/build -j $NPROC llc clang && ninja -C deps/llvm-source/build install"
