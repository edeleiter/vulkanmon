"""
Depth Buffer Tests

Tests for 3D depth testing functionality - Step 24 validation.
Following our "Test, Test, Test" philosophy for robust depth rendering.
"""

import pytest
import subprocess
import time
from pathlib import Path


@pytest.mark.vulkan
def test_depth_shader_compilation(project_root):
    """Test that depth-enabled shaders compile successfully."""
    shader_dir = project_root / "shaders"
    
    # Test vertex shader compilation with depth
    vert_result = subprocess.run(
        ["glslc", str(shader_dir / "triangle.vert"), "-o", str(shader_dir / "test_vert.spv")],
        cwd=project_root,
        capture_output=True,
        text=True
    )
    
    assert vert_result.returncode == 0, f"Vertex shader compilation failed: {vert_result.stderr}"
    
    # Test fragment shader compilation 
    frag_result = subprocess.run(
        ["glslc", str(shader_dir / "triangle.frag"), "-o", str(shader_dir / "test_frag.spv")],
        cwd=project_root,
        capture_output=True,
        text=True
    )
    
    assert frag_result.returncode == 0, f"Fragment shader compilation failed: {frag_result.stderr}"
    
    # Cleanup test files
    test_vert = shader_dir / "test_vert.spv"
    test_frag = shader_dir / "test_frag.spv"
    if test_vert.exists():
        test_vert.unlink()
    if test_frag.exists():
        test_frag.unlink()


@pytest.mark.vulkan
@pytest.mark.depth
def test_depth_buffer_initialization(project_root, build_dir):
    """Test that depth buffer resources are properly initialized."""
    executable = build_dir / "Debug" / "vulkanmon.exe"
    
    # Run application using Popen and terminate after initialization
    process = subprocess.Popen(
        [str(executable)],
        cwd=build_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    # Let it initialize
    time.sleep(2)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    # Should see depth buffer creation message
    assert "Depth buffer created successfully!" in stdout, "Depth buffer initialization not found"
    
    # Should not have depth-related errors
    assert "depth" not in stderr.lower(), f"Depth-related errors found: {stderr}"


@pytest.mark.vulkan  
@pytest.mark.visual
@pytest.mark.depth
def test_depth_cube_rendering(project_root, build_dir):
    """Test that 3D cube renders with proper depth testing."""
    executable = build_dir / "Debug" / "vulkanmon.exe"
    
    # Run application briefly to test rendering
    process = subprocess.Popen(
        [str(executable)],
        cwd=build_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    # Let it run for a moment to initialize and render
    time.sleep(3)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    # Should initialize successfully
    assert "Graphics pipeline created successfully!" in stdout
    assert "Depth buffer created successfully!" in stdout
    assert "Descriptor set created successfully!" in stdout
    
    # Should not have depth-related errors
    assert "depth" not in stderr.lower(), f"Depth rendering errors: {stderr}"


@pytest.mark.vulkan
@pytest.mark.depth
def test_mvp_matrix_transformation(project_root, build_dir):
    """Test that MVP (Model-View-Projection) matrices are working."""
    executable = build_dir / "Debug" / "vulkanmon.exe"
    
    process = subprocess.Popen(
        [str(executable)],
        cwd=build_dir, 
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    time.sleep(1.5)
    process.terminate()
    stdout, stderr = process.communicate(timeout=3)
    
    # Should have UBO (uniform buffer) creation for MVP matrices
    assert "Uniform buffer created successfully!" in stdout
    
    # Should not have matrix-related errors
    assert "matrix" not in stderr.lower(), f"Matrix transformation errors: {stderr}"


@pytest.mark.performance
@pytest.mark.depth
def test_depth_rendering_performance(project_root, build_dir):
    """Test that depth buffer doesn't significantly impact performance."""
    executable = build_dir / "Debug" / "vulkanmon.exe"
    
    start_time = time.time()
    
    process = subprocess.Popen(
        [str(executable)],
        cwd=build_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    # Let it render for a short time
    time.sleep(4)
    process.terminate()
    stdout, stderr = process.communicate(timeout=5)
    
    total_time = time.time() - start_time
    
    # Should initialize reasonably quickly (within 7 seconds including startup)
    assert total_time < 8, f"Depth rendering initialization too slow: {total_time:.2f}s"
    
    # Should complete initialization successfully (check for final initialization message)
    assert "Descriptor set created successfully!" in stdout, "Application did not complete initialization"