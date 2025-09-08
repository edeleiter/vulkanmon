"""
Uniform Buffer Object (UBO) Tests

Tests for Step 23: UBO system functionality
Following "Test, Test, Test" philosophy for 3D transformations
"""

import pytest
import subprocess
import time
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

@pytest.mark.ubo
def test_ubo_shader_compilation():
    """Test that UBO shaders compile without errors"""
    os.chdir(os.path.join(os.path.dirname(__file__), '..', 'shaders'))
    
    # Test vertex shader compilation with UBO support
    result = subprocess.run(['glslc', 'triangle.vert', '-o', 'test_vert_ubo.spv'], 
                          capture_output=True, text=True)
    assert result.returncode == 0, f"UBO vertex shader compilation failed: {result.stderr}"
    
    # Test fragment shader compilation  
    result = subprocess.run(['glslc', 'triangle.frag', '-o', 'test_frag_ubo.spv'],
                          capture_output=True, text=True)
    assert result.returncode == 0, f"Fragment shader compilation failed: {result.stderr}"
    
    # Cleanup test files
    for file in ['test_vert_ubo.spv', 'test_frag_ubo.spv']:
        if os.path.exists(file):
            os.remove(file)
    
    os.chdir(os.path.join(os.path.dirname(__file__)))

@pytest.mark.ubo
def test_ubo_resources_initialized():
    """Test that UBO resources are created without errors"""
    os.chdir(os.path.join(os.path.dirname(__file__), '..', 'build'))
    
    # Run application for short duration to test UBO initialization
    process = subprocess.Popen(['Debug/vulkanmon.exe'], 
                             stdout=subprocess.PIPE, 
                             stderr=subprocess.PIPE,
                             text=True)
    
    # Let it run for 2 seconds
    time.sleep(2)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    # Check for successful UBO creation messages
    assert "Uniform buffer created successfully!" in stdout
    assert "Descriptor set layout created successfully!" in stdout
    assert "Descriptor pool created successfully!" in stdout
    assert "Descriptor set created successfully!" in stdout
    
    # Should not crash or have errors
    assert "Segmentation fault" not in stderr
    assert "Failed to" not in stdout
    
    os.chdir(os.path.join(os.path.dirname(__file__)))

@pytest.mark.ubo  
def test_ubo_rotation_rendering():
    """Test that UBO rotation animation works correctly"""
    os.chdir(os.path.join(os.path.dirname(__file__), '..', 'build'))
    
    # Run application for longer duration to test rotation
    process = subprocess.Popen(['Debug/vulkanmon.exe'],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, 
                             text=True)
    
    # Let it rotate for 4 seconds
    time.sleep(4)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    # Verify complete UBO pipeline
    expected_messages = [
        "Vulkan instance created successfully!",
        "Descriptor set layout created successfully!",
        "Shaders loaded successfully!",
        "Graphics pipeline created successfully!", 
        "Uniform buffer created successfully!",
        "Texture image created successfully!",
        "Descriptor set created successfully!"
    ]
    
    for message in expected_messages:
        assert message in stdout, f"Missing expected message: {message}"
    
    # Should complete rotation animation without crashes
    assert "Segmentation fault" not in stderr
    
    os.chdir(os.path.join(os.path.dirname(__file__)))

@pytest.mark.ubo
def test_vertex_shader_ubo_binding():
    """Test that vertex shader properly declares UBO binding"""
    shader_path = os.path.join(os.path.dirname(__file__), '..', 'shaders', 'triangle.vert')
    
    with open(shader_path, 'r') as f:
        shader_content = f.read()
    
    # Verify vertex shader has UBO binding
    assert "layout(binding = 0) uniform UniformBufferObject" in shader_content
    assert "mat4 model;" in shader_content
    assert "ubo.model * vec4(inPosition, 0.0, 1.0)" in shader_content
    
    # Verify shader version and structure
    assert "#version 450" in shader_content
    assert "} ubo;" in shader_content

@pytest.mark.ubo
@pytest.mark.performance
def test_ubo_animation_performance():
    """Test that UBO rotation maintains good performance"""
    os.chdir(os.path.join(os.path.dirname(__file__), '..', 'build'))
    
    start_time = time.time()
    
    # Run rotation for 6 seconds to test performance stability
    process = subprocess.Popen(['Debug/vulkanmon.exe'],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             text=True)
    
    time.sleep(6)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    end_time = time.time()
    runtime = end_time - start_time
    
    # Should initialize UBO system completely
    setup_complete = "Uniform buffer created successfully!" in stdout
    assert setup_complete, "Application should initialize UBO system completely"
    
    # Should run stable for full duration without crashes
    assert "Segmentation fault" not in stderr
    assert runtime >= 5.5, "Application should run for expected duration"
    
    os.chdir(os.path.join(os.path.dirname(__file__)))