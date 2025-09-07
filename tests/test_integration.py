"""
Integration Tests

End-to-end testing of the complete build and run cycle.
Following our "Simple is Powerful" philosophy.
"""

import pytest
import subprocess
import os


@pytest.mark.integration
def test_full_build_cycle(project_root, build_dir, executable_path):
    """Test complete build cycle from source to executable."""
    # Ensure we're in the right directory
    os.chdir(build_dir)
    
    # Configure
    cmake_cmd = [
        "cmake", "..", 
        "-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake"
    ]
    configure_result = subprocess.run(cmake_cmd, capture_output=True, text=True)
    assert configure_result.returncode == 0, f"Configure failed: {configure_result.stderr}"
    
    # Build
    build_result = subprocess.run(["cmake", "--build", "."], capture_output=True, text=True)
    assert build_result.returncode == 0, f"Build failed: {build_result.stderr}"
    
    # Verify executable
    assert executable_path.exists(), "Executable not created after build"
    
    # Test run
    try:
        result = subprocess.run(
            [str(executable_path)],
            timeout=3,
            capture_output=True,
            text=True,
            cwd=project_root  # Run from project root so shaders are found
        )
        # If it exits, check that it at least started Vulkan initialization
        if result.returncode != 0:
            output = result.stdout + result.stderr
            assert "Vulkan instance created successfully!" in output, "Build succeeded but Vulkan init failed"
    except subprocess.TimeoutExpired:
        # Expected behavior - app runs indefinitely after successful init
        pass


@pytest.mark.integration
def test_incremental_build(build_dir, executable_path):
    """Test that incremental builds work correctly."""
    if not build_dir.exists() or not executable_path.exists():
        pytest.skip("Initial build not present - run full build test first")
    
    os.chdir(build_dir)
    
    # Record original modification time
    original_mtime = executable_path.stat().st_mtime
    
    # Run incremental build
    result = subprocess.run(["cmake", "--build", "."], capture_output=True, text=True)
    assert result.returncode == 0, f"Incremental build failed: {result.stderr}"
    
    # Executable should still exist
    assert executable_path.exists(), "Executable missing after incremental build"


@pytest.mark.performance
def test_build_performance(project_root, build_dir):
    """Test that builds complete in reasonable time."""
    import time
    
    os.chdir(build_dir)
    
    start_time = time.time()
    result = subprocess.run(["cmake", "--build", "."], capture_output=True, text=True)
    build_time = time.time() - start_time
    
    assert result.returncode == 0, "Build must succeed for performance test"
    assert build_time < 120, f"Build took too long: {build_time:.2f}s (max 120s)"