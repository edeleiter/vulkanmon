"""
VulkanMon Testing Configuration

Following our "Test, Test, Test" philosophy with proper pytest setup.
Provides fixtures and utilities for comprehensive testing.
"""

import pytest
import subprocess
import sys
import os
from pathlib import Path

# Project root directory
PROJECT_ROOT = Path(__file__).parent.parent

@pytest.fixture(scope="session")
def project_root():
    """Provide project root path for all tests."""
    return PROJECT_ROOT

@pytest.fixture(scope="session")
def build_dir(project_root):
    """Provide build directory path."""
    return project_root / "build"

@pytest.fixture(scope="session")
def executable_path(build_dir):
    """Provide path to vulkanmon executable."""
    return build_dir / "Debug" / "vulkanmon.exe"

@pytest.fixture(scope="session")
def vcpkg_dir(project_root):
    """Provide vcpkg directory path."""
    return project_root / "vcpkg"

class VulkanMonRunner:
    """Helper class for running VulkanMon executable with proper timeout handling."""
    
    def __init__(self, executable_path):
        self.executable_path = executable_path
    
    def run_with_timeout(self, timeout=2):
        """
        Run VulkanMon with timeout.
        
        Returns:
            tuple: (success: bool, output: str, error: str)
        """
        if not self.executable_path.exists():
            return False, "", f"Executable not found: {self.executable_path}"
        
        try:
            result = subprocess.run(
                [str(self.executable_path)],
                timeout=timeout,
                capture_output=True,
                text=True,
                cwd=self.executable_path.parent.parent  # Run from project root
            )
            return True, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            # Timeout is expected for the main app
            return True, "", ""
        except Exception as e:
            return False, "", str(e)

@pytest.fixture
def vulkanmon_runner(executable_path):
    """Provide VulkanMon runner utility."""
    return VulkanMonRunner(executable_path)