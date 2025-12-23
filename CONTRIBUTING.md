# Contributing to TerminalDX12

Thank you for your interest in contributing to TerminalDX12! We welcome contributions from everyone.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/TerminalDX12.git
   ```
3. **Set up the development environment** following [BUILD.md](BUILD.md)
4. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Pull Request Process

1. **All PRs are welcome** - bug fixes, features, documentation, tests
2. Ensure your code builds without errors
3. Run the test suite before submitting:
   ```powershell
   # C++ tests
   .\build\tests\Release\TerminalDX12Tests.exe

   # Python tests
   python -m pytest tests/ -v
   ```
4. Update documentation if you're changing functionality
5. Submit your PR with a clear description of changes

## Code Style

### C++ Guidelines

- **Modern C++**: Use C++20 features (the project requires C++20)
- **Naming**:
  - Classes: `PascalCase` (e.g., `TextRenderer`)
  - Methods: `PascalCase` (e.g., `RenderGlyph`)
  - Variables: `camelCase` (e.g., `bufferIndex`)
  - Constants: `SCREAMING_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`)
  - Member variables: `m_` prefix (e.g., `m_width`)
- **Formatting**:
  - 4-space indentation
  - Braces on same line for functions
  - Max line length: 120 characters
- **Memory**: Use smart pointers (`std::unique_ptr`, `std::shared_ptr`), avoid raw `new/delete`
- **Error handling**: Use exceptions for exceptional cases, return values for expected failures

### Python Guidelines (Tests)

- Follow PEP 8
- Use type hints where practical
- Use pytest for testing

### Commit Messages

- Use present tense: "Add feature" not "Added feature"
- First line: 50 chars max, summary of change
- Body: Explain what and why (not how)

Example:
```
Add mouse wheel scrolling support

Implements smooth scrolling via WM_MOUSEWHEEL messages.
Scrollback buffer is now navigable with the mouse wheel.
```

## What to Contribute

### Good First Issues

- Documentation improvements
- Test coverage expansion
- Bug fixes with clear reproduction steps
- Code cleanup and refactoring

### Feature Ideas

- Additional VT escape sequence support
- Configuration file support
- Theme customization
- Performance optimizations

## Development Tips

### Building Debug Version

```powershell
cmake --build build --config Debug
```

### Running with Sanitizers

Enable address sanitizer in CMakeLists.txt for memory debugging.

### Debugging DirectX

Use PIX for Windows or Visual Studio Graphics Debugger for GPU debugging.

## Questions?

- Open an issue for bugs or feature requests
- Start a discussion for questions or ideas

## License

By contributing, you agree that your contributions will be licensed under the GPL v2 License.
