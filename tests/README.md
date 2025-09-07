# VulkanMon Testing Suite

Modern pytest-based testing framework following our **"Test, Test, Test"** philosophy.

## Quick Start

```bash
# From the tests directory:
cd tests

# Install dependencies:
pip install -r requirements.txt

# Run all tests:
python -m pytest

# Run with verbose output:
python -m pytest -v

# Run specific test categories:
python -m pytest -m build        # Build system tests
python -m pytest -m vulkan       # Vulkan runtime tests  
python -m pytest -m integration  # End-to-end tests
python -m pytest -m performance  # Performance tests
```

## Test Structure

- **`test_build_system.py`** - CMake configuration and build validation
- **`test_file_structure.py`** - Project structure and required files
- **`test_vulkan_runtime.py`** - Vulkan initialization and runtime tests  
- **`test_integration.py`** - End-to-end workflow testing with performance checks
- **`conftest.py`** - Pytest fixtures and utilities

## Setup

The testing environment uses:
- **Python 3.8+** with pip package management
- **pytest** with markers for test categorization
- **Simple requirements.txt** for dependency management
- **Comprehensive fixtures** for project paths and utilities

## Test Philosophy

Following VulkanMon's core tenants:

- 🎯 **"Simple is Powerful"** - Clear, focused tests that validate one thing well
- 🧪 **"Test, Test, Test"** - Comprehensive coverage of build, runtime, and integration
- 📚 **"Document Often"** - Well-documented test cases that serve as living documentation

## Test Markers

Use markers to run specific test categories:

- `@pytest.mark.build` - Build system and compilation tests
- `@pytest.mark.vulkan` - Vulkan API and runtime tests
- `@pytest.mark.integration` - End-to-end workflow tests  
- `@pytest.mark.performance` - Performance validation tests

Run tests by marker: `python -m pytest -m build`