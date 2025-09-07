"""
File Structure Tests

Validates project file organization and required files.
Following our "Document Often" philosophy.
"""

import pytest
from pathlib import Path


@pytest.mark.build
def test_source_files(project_root):
    """Test that all required source files exist."""
    required_source_files = [
        "src/main.cpp",
    ]
    
    for file_path in required_source_files:
        full_path = project_root / file_path
        assert full_path.exists(), f"Missing source file: {file_path}"
        assert full_path.is_file(), f"Source path is not a file: {file_path}"


@pytest.mark.build
def test_shader_files(project_root):
    """Test that all required shader files exist."""
    required_shader_files = [
        "shaders/triangle.vert",
        "shaders/triangle.frag", 
        "shaders/triangle_vert.spv",
        "shaders/triangle_frag.spv",
    ]
    
    for file_path in required_shader_files:
        full_path = project_root / file_path
        assert full_path.exists(), f"Missing shader file: {file_path}"
        assert full_path.is_file(), f"Shader path is not a file: {file_path}"


@pytest.mark.build
def test_build_files(project_root):
    """Test that all required build files exist."""
    required_build_files = [
        "CMakeLists.txt",
    ]
    
    for file_path in required_build_files:
        full_path = project_root / file_path
        assert full_path.exists(), f"Missing build file: {file_path}"
        assert full_path.is_file(), f"Build path is not a file: {file_path}"


@pytest.mark.build  
def test_documentation_files(project_root):
    """Test that all required documentation files exist."""
    required_doc_files = [
        "README.md",
        "TODO.md", 
        "PROGRESS.md",
        "BUILD.md",
        "DEVELOPMENT_PHILOSOPHY.md",
        "TESTING.md"
    ]
    
    for file_path in required_doc_files:
        full_path = project_root / file_path
        assert full_path.exists(), f"Missing documentation file: {file_path}"
        assert full_path.is_file(), f"Documentation path is not a file: {file_path}"


@pytest.mark.build
def test_vcpkg_structure(vcpkg_dir):
    """Test that vcpkg is properly set up."""
    assert vcpkg_dir.exists(), "vcpkg directory not found"
    assert vcpkg_dir.is_dir(), "vcpkg path is not a directory"
    
    vcpkg_exe = vcpkg_dir / "vcpkg.exe"
    assert vcpkg_exe.exists(), "vcpkg.exe not found"