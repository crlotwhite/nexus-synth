#!/bin/bash
# migrate_dependencies.sh - Script to migrate from git submodules to CPM

set -e  # Exit on error

echo "=== NexusSynth Dependency Migration Script ==="
echo "Migrating from git submodules to CPM (CMake Package Manager)"
echo

# Check if we're in the right directory
if [[ ! -f "CMakeLists.txt" ]] || [[ ! -d "external" ]]; then
    echo "Error: Please run this script from the NexusSynth root directory"
    exit 1
fi

# Backup the current external directory
echo "1. Creating backup of external/ directory..."
if [[ -d "external_backup" ]]; then
    echo "   Warning: external_backup already exists, removing old backup..."
    rm -rf external_backup
fi

cp -r external external_backup
echo "   ✓ Backup created at external_backup/"

# Remove git submodules
echo "2. Removing git submodules..."

# Check if .gitmodules exists
if [[ -f ".gitmodules" ]]; then
    echo "   Found .gitmodules, removing submodules..."
    
    # Read submodules and remove them
    git submodule deinit --all -f
    rm -rf .git/modules/external
    
    # Remove from git index
    git rm -f external/WORLD external/Eigen external/asmjit external/cJSON 2>/dev/null || true
    
    # Remove .gitmodules
    rm -f .gitmodules
    
    echo "   ✓ Git submodules removed"
else
    echo "   No .gitmodules found, skipping git submodule cleanup"
fi

# Remove the external directory
echo "3. Removing external/ directory..."
rm -rf external
echo "   ✓ external/ directory removed"

# Create .cpm_cache directory for offline builds
echo "4. Creating CPM cache directory..."
mkdir -p .cpm_cache
echo "   ✓ .cpm_cache directory created"

# Add .cpm_cache to .gitignore if not already present
echo "5. Updating .gitignore..."
if [[ ! -f ".gitignore" ]]; then
    touch .gitignore
fi

if ! grep -q ".cpm_cache" .gitignore; then
    echo ".cpm_cache/" >> .gitignore
    echo "   ✓ Added .cpm_cache/ to .gitignore"
else
    echo "   .cpm_cache already in .gitignore"
fi

# Test the new build system
echo "6. Testing new build system..."
echo "   Creating test build directory..."

# Clean up any existing build directory
if [[ -d "build_test" ]]; then
    rm -rf build_test
fi

mkdir build_test
cd build_test

echo "   Running cmake configuration..."
if cmake -DNEXUSSYNTH_BUILD_TESTS=OFF .. > cmake_output.log 2>&1; then
    echo "   ✓ CMake configuration successful"
    
    echo "   Testing compile (building nexussynth_lib only)..."
    if make nexussynth_lib -j$(nproc) >> cmake_output.log 2>&1; then
        echo "   ✓ Library compilation successful"
    else
        echo "   ✗ Library compilation failed"
        echo "   Check cmake_output.log for details"
        cd ..
        exit 1
    fi
else
    echo "   ✗ CMake configuration failed"
    echo "   Check cmake_output.log for details"
    cd ..
    exit 1
fi

cd ..
rm -rf build_test

echo
echo "=== Migration completed successfully! ==="
echo
echo "Key changes:"
echo "  ✓ Removed git submodules (WORLD, Eigen, AsmJit, cJSON)"
echo "  ✓ Added CPM (CMake Package Manager) support"
echo "  ✓ Dependencies now auto-download on first build"
echo "  ✓ Simplified build process (just 'cmake && make')"
echo
echo "New build process:"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make -j\$(nproc)"
echo
echo "For offline builds, dependencies will be cached in .cpm_cache/"
echo "The backup is available at external_backup/ if you need to revert"
echo
echo "Next steps:"
echo "  1. Test the build: mkdir build && cd build && cmake .. && make"
echo "  2. Commit the changes: git add . && git commit -m \"feat: migrate to CPM dependency management\""
echo "  3. Remove external_backup/ when satisfied: rm -rf external_backup"
echo