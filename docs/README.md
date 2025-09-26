# VulkanMon Documentation

This directory contains VulkanMon's documentation system and generated API documentation.

## 📚 Documentation Types

### API Documentation (Doxygen)
Professional API documentation generated from source code comments using Doxygen.

**Location**: `docs/api/html/index.html` (after generation)
**Source**: Doxygen comments in `src/` directory
**Configuration**: `Doxyfile` in project root

### Development Documentation
Hand-written documentation covering architecture, build processes, and development workflows.

**Location**: `docs/` directory (markdown files)
**Source**: Manual documentation files

## 🔧 Generating Documentation

### Prerequisites
- **Doxygen**: Install from [doxygen.nl](https://www.doxygen.nl/download.html)
- **CMake**: 3.20+ (already required for VulkanMon)
- **Fresh Terminal**: Restart terminal after Doxygen installation

### Quick Generation

#### Option 1: CMake Build System
```bash
# From build directory
cmake --build . --target docs

# Open documentation (Windows)
cmake --build . --target docs-open
```

#### Option 2: Convenience Script
```bash
# From build directory
..\generate_docs.bat
```

#### Option 3: Direct Doxygen
```bash
# From project root
doxygen Doxyfile
```

### Build Configuration

Documentation generation is controlled by CMake option:
```bash
# Enable documentation (default)
cmake .. -DBUILD_DOCS=ON

# Disable documentation
cmake .. -DBUILD_DOCS=OFF
```

## 📁 Documentation Structure

```
docs/
├── README.md                 # This file
├── api/                      # Generated Doxygen documentation
│   └── html/
│       └── index.html        # Main API documentation page
├── PHASE_*_PLAN.md          # Development phase documentation
├── BUILD_*.md               # Build system documentation
├── CROSS_PLATFORM_*.md      # Platform-specific guides
└── images/                  # Documentation images
```

## 🎯 Documentation Standards

### Code Documentation
All public APIs should include Doxygen comments with:
- `@brief`: Short description
- `@details`: Detailed explanation if needed
- `@param`: All parameters with descriptions
- `@return`: Return value description
- `@throws`: Exception information
- `@see`: Cross-references to related items
- `@example`: Usage examples for complex APIs

### Example
```cpp
/**
 * @brief Initialize all engine systems
 * @details Must be called before run(). Initializes Window, VulkanRenderer,
 *          ECS World, and connects all subsystems for stable operation.
 * @throws std::runtime_error if initialization fails
 * @see run()
 */
void initialize();
```

## 🔍 Documentation Quality

### Current Status
- **Application.h**: ✅ Complete Doxygen documentation
- **PhysicsSystem.h**: ✅ Complete Doxygen documentation
- **Core Components**: ✅ Most components documented
- **Rendering System**: 🔄 Partial documentation
- **ECS Systems**: 🔄 Partial documentation

### Quality Metrics
- **Classes Documented**: 15+ core classes with @brief/@details
- **Methods Documented**: 25+ public methods with @param/@return
- **Cross-References**: 20+ @see tags linking related concepts
- **Usage Examples**: 5+ @example blocks with working code

## 🚀 Advanced Features

### Graphical Diagrams
Enable class and collaboration diagrams by installing Graphviz:
```bash
# vcpkg
vcpkg install graphviz

# Update Doxyfile
HAVE_DOT = YES
```

### PDF Generation
Enable LaTeX/PDF output:
```bash
# Update Doxyfile
GENERATE_LATEX = YES
USE_PDFLATEX = YES
```

### Custom Styling
Customize documentation appearance:
- **HTML Theme**: Modify `HTML_EXTRA_STYLESHEET` in Doxyfile
- **Project Logo**: Set `PROJECT_LOGO` in Doxyfile
- **Custom Header/Footer**: Use `HTML_HEADER`/`HTML_FOOTER`

## 🔧 Troubleshooting

### Common Issues

#### "Doxygen not found"
**Solution**: Install Doxygen and restart terminal
```bash
# Verify installation
doxygen --version
```

#### "Documentation generation failed"
**Solutions**:
1. Check Doxygen syntax in source files
2. Verify file paths in Doxyfile
3. Ensure source files exist and are readable

#### "Empty or missing documentation"
**Solutions**:
1. Add more Doxygen comments to source files
2. Check `EXTRACT_ALL` setting in Doxyfile
3. Verify `INPUT` directories are correct

#### "Broken cross-references"
**Solutions**:
1. Verify @see references point to existing classes/methods
2. Check namespace and class name spelling
3. Ensure referenced files are included in INPUT

### Performance Tips
- Use `EXTRACT_ALL = NO` for faster generation (documented items only)
- Disable `SOURCE_BROWSER` if not needed
- Limit `DOT_GRAPH_MAX_NODES` for large codebases

## 📈 Future Enhancements

### Planned Improvements
- **CI/CD Integration**: Automatic documentation deployment
- **API Versioning**: Multiple documentation versions
- **Interactive Examples**: Embedded code playground
- **Search Integration**: Advanced search functionality
- **Mobile Optimization**: Responsive documentation themes

### Contributing
To improve documentation:
1. Add Doxygen comments to undocumented code
2. Fix broken cross-references
3. Add usage examples for complex APIs
4. Update this README with new procedures

---

**VulkanMon Documentation System v7.1**
*Professional API documentation for modern C++20 game engine*