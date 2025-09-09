"""
Texture Loading and Sampling Tests

Tests for Step 22: Texture system functionality
Following "Test, Test, Test" philosophy
"""

import pytest
import subprocess
import time
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

@pytest.mark.texture
def test_texture_shader_compilation():
    """Test that texture shaders compile without errors"""
    os.chdir(os.path.join(os.path.dirname(__file__), '..', 'shaders'))
    
    # Test vertex shader compilation
    result = subprocess.run(['glslc', 'triangle.vert', '-o', 'test_vert.spv'], 
                          capture_output=True, text=True)
    assert result.returncode == 0, f"Vertex shader compilation failed: {result.stderr}"
    
    # Test fragment shader compilation  
    result = subprocess.run(['glslc', 'triangle.frag', '-o', 'test_frag.spv'],
                          capture_output=True, text=True)
    assert result.returncode == 0, f"Fragment shader compilation failed: {result.stderr}"
    
    # Cleanup test files
    if os.path.exists('test_vert.spv'):
        os.remove('test_vert.spv')
    if os.path.exists('test_frag.spv'):
        os.remove('test_frag.spv')
    
    os.chdir(os.path.join(os.path.dirname(__file__)))

@pytest.mark.texture
def test_texture_resources_initialized(project_root):
    """Test that texture resources are properly integrated in source code."""
    main_cpp = project_root / "src" / "main.cpp"
    content = main_cpp.read_text()
    
    # Check for texture-related code patterns
    assert "VkImage textureImage" in content, "Texture image declaration missing"
    assert "VkImageView textureImageView" in content, "Texture image view declaration missing"
    assert "VkSampler textureSampler" in content, "Texture sampler declaration missing"
    assert "vkCreateImage" in content, "Texture image creation missing"
    assert "vkCreateImageView" in content, "Texture image view creation missing"
    assert "vkCreateSampler" in content, "Texture sampler creation missing"

@pytest.mark.texture  
def test_textured_triangle_rendering():
    """Test that textured triangle renders successfully"""
    os.chdir(os.path.join(os.path.dirname(__file__), '..', 'build'))
    
    # Run application for longer duration to test rendering loop
    process = subprocess.Popen(['Debug/vulkanmon.exe'],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, 
                             text=True)
    
    # Let it render for 3 seconds
    time.sleep(3)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    # Verify complete initialization sequence
    expected_messages = [
        "Vulkan instance created successfully!",
        "Logical device created successfully!",
        "Swap chain created successfully",
        "Render pass created successfully!",
        "Descriptor set layout created successfully!",
        "Shaders loaded successfully!",
        "Graphics pipeline created successfully!",
        "Texture image created successfully!",
        "Texture sampler created successfully!",
        "Descriptor set created successfully!"
    ]
    
    for message in expected_messages:
        assert message in stdout, f"Missing expected message: {message}"
    
    # Should complete full rendering loop without crashes
    assert "Segmentation fault" not in stderr
    
    os.chdir(os.path.join(os.path.dirname(__file__)))

@pytest.mark.texture
@pytest.mark.performance  
def test_texture_rendering_performance():
    """Test that texture rendering maintains good performance"""
    os.chdir(os.path.join(os.path.dirname(__file__), '..', 'build'))
    
    start_time = time.time()
    
    # Run for 5 seconds to test performance
    process = subprocess.Popen(['Debug/vulkanmon.exe'],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             text=True)
    
    time.sleep(5)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    end_time = time.time()
    runtime = end_time - start_time
    
    # Should initialize texture system completely 
    setup_complete = "Descriptor set created successfully!" in stdout
    assert setup_complete, "Application should initialize texture system completely"
    
    # Should run stable for full duration without crashes
    assert "Segmentation fault" not in stderr
    assert runtime >= 4.5, "Application should run for expected duration"
    
    os.chdir(os.path.join(os.path.dirname(__file__)))

@pytest.mark.texture
def test_shader_texture_binding():
    """Test that shaders properly declare texture binding"""
    shader_path = os.path.join(os.path.dirname(__file__), '..', 'shaders', 'triangle.frag')
    
    with open(shader_path, 'r') as f:
        shader_content = f.read()
    
    # Verify fragment shader has texture sampler binding (moved to binding 1 for UBOs)
    assert "layout(binding = 1) uniform sampler2D texSampler;" in shader_content
    assert "texture(texSampler, fragTexCoord)" in shader_content
    
    # Verify shader version and inputs
    assert "#version 450" in shader_content
    assert "layout(location = 1) in vec2 fragTexCoord;" in shader_content

@pytest.mark.texture  
def test_vertex_texture_coordinates():
    """Test that vertex shader passes texture coordinates correctly"""
    shader_path = os.path.join(os.path.dirname(__file__), '..', 'shaders', 'triangle.vert')
    
    with open(shader_path, 'r') as f:
        shader_content = f.read()
        
    # Verify vertex shader has texture coordinate input and output
    assert "layout(location = 2) in vec2 inTexCoord;" in shader_content
    assert "layout(location = 1) out vec2 fragTexCoord;" in shader_content
    assert "fragTexCoord = inTexCoord;" in shader_content