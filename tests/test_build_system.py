"""
Build System Tests

Tests for CMake configuration, compilation, and executable generation.
Following our "Test, Test, Test" philosophy.
"""

import pytest
import subprocess
import os
from pathlib import Path


@pytest.mark.build
def test_cmake_configure(project_root, build_dir):
    """Test CMake configuration with vcpkg toolchain."""
    if not build_dir.exists():
        build_dir.mkdir()
    
    os.chdir(build_dir)
    
    cmake_cmd = [
        "cmake", "..", 
        "-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake"
    ]
    
    result = subprocess.run(cmake_cmd, capture_output=True, text=True)
    assert result.returncode == 0, f"CMake configure failed: {result.stderr}"


@pytest.mark.build
def test_cmake_build(build_dir, executable_path):
    """Test CMake build process."""
    os.chdir(build_dir)
    
    result = subprocess.run(["cmake", "--build", "."], capture_output=True, text=True)
    assert result.returncode == 0, f"Build failed: {result.stderr}"
    assert executable_path.exists(), f"Executable not found: {executable_path}"


@pytest.mark.build
def test_executable_exists(executable_path):
    """Verify the vulkanmon executable was created."""
    assert executable_path.exists(), f"Executable not found: {executable_path}"
    assert executable_path.is_file(), "Executable path is not a file"