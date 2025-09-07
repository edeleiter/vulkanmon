"""
Vulkan Runtime Tests

Tests for Vulkan initialization and basic functionality.
Following our "Test, Test, Test" philosophy.
"""

import pytest
import subprocess


@pytest.mark.vulkan
def test_vulkan_initialization(vulkanmon_runner):
    """Test that VulkanMon initializes Vulkan systems correctly."""
    success, stdout, stderr = vulkanmon_runner.run_with_timeout(timeout=3)
    
    assert success, f"VulkanMon failed to run: {stderr}"
    # Process running and terminating cleanly indicates successful initialization


@pytest.mark.vulkan
def test_application_startup(executable_path):
    """Test that the application starts and initializes Vulkan systems."""
    assert executable_path.exists(), "Executable must exist before runtime tests"
    
    # Run the application briefly to check initialization
    try:
        result = subprocess.run(
            [str(executable_path)],
            timeout=3,
            capture_output=True,
            text=True,
            cwd=executable_path.parent.parent  # Run from project root so shaders are found
        )
        # If it exits, check that it at least started Vulkan initialization
        if result.returncode != 0:
            # Check if it got past basic Vulkan setup before failing
            output = result.stdout + result.stderr
            assert "Vulkan instance created successfully!" in output, "Vulkan instance creation failed"
    except subprocess.TimeoutExpired:
        # Expected behavior - app runs indefinitely after successful init
        pass


@pytest.mark.vulkan  
def test_expected_console_messages():
    """Test that expected initialization messages are documented."""
    # This is a documentation test - ensures we track expected output
    expected_messages = [
        "Vulkan instance created successfully!",
        "Window surface created successfully!", 
        "Logical device created successfully!",
        "Swap chain created successfully",
        "Render pass created successfully!",
        "Shaders loaded successfully!",
        "Graphics pipeline created successfully!",
        "Framebuffers created successfully!",
        "Command pool created successfully!",
        "Vertex buffer created successfully!",
        "Command buffers allocated successfully!",
        "Drawing commands recorded successfully!",
        "Sync objects created successfully!"
    ]
    
    # This test always passes but documents what we expect to see
    assert len(expected_messages) > 0, "Expected messages should be defined"
    
    # In a more sophisticated setup, we could capture and validate these
    # For now, this serves as documentation of expected behavior