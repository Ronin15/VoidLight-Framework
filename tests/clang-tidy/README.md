# clang-tidy Configuration for VoidLight-Framework

This directory contains clang-tidy configuration for comprehensive static analysis.
clang-tidy uses the Clang compiler frontend for accurate cross-translation-unit analysis.

## Quick Start

### Prerequisites
```bash
# Install clang-tidy (macOS)
brew install llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

# Verify installation
clang-tidy --version

# Add to shell profile for persistence
echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
```

### Running Analysis

#### Focused Analysis (Project `src/` Files) - Recommended for Development
```bash
cd tests/clang-tidy
./clang_tidy_focused.sh
```

The focused script filters the compile database to first-party project files under
`src/`. It is not a git-diff changed-file runner.

#### Full Analysis (All Source Files) - Comprehensive but Slower
```bash
cd tests/clang-tidy
./run_clang_tidy.sh
```

#### Via repository root
```bash
./tests/clang-tidy/run_clang_tidy.sh
```

## Configuration

### `.clang-tidy`
Main configuration file with:
- Enabled check categories (bugprone, performance, modernize, etc.)
- Disabled checks that conflict with project style
- Naming conventions matching `AGENTS.md` standards

### Check Categories
| Category | Purpose |
|----------|---------|
| bugprone-* | Common bug patterns (null dereference, use-after-move) |
| clang-analyzer-* | Deep static analysis (memory leaks, dead stores) |
| cppcoreguidelines-* | C++ Core Guidelines compliance |
| modernize-* | Modern C++ suggestions (auto, nullptr, range-for) |
| performance-* | Performance issues (unnecessary copies, inefficient algorithms) |
| readability-* | Code readability (naming, braces, simplifications) |
| misc-* | Miscellaneous checks |

### Disabled Checks (Project-Specific)
| Check | Reason |
|-------|--------|
| `modernize-use-trailing-return-type` | Style preference |
| `readability-magic-numbers` | Game engines use many constants |
| `cppcoreguidelines-avoid-non-const-global-variables` | Singletons |
| `cppcoreguidelines-owning-memory` | Smart pointers used differently |
| `misc-non-private-member-variables-in-classes` | POD structs |
| `bugprone-easily-swappable-parameters` | Too many false positives |

### Naming Convention Enforcement
Configured to match `AGENTS.md` standards:
- **Classes/Structs/Enums**: `CamelCase`
- **Functions/Methods**: `camelBack`
- **Variables/Parameters**: `camelBack`
- **Private Members**: `m_camelBack`
- **Constants**: `UPPER_CASE`
- **Namespaces**: `lower_case`

## Integration with cppcheck

Both tools complement each other:

| Feature | cppcheck | clang-tidy |
|---------|----------|------------|
| Speed | Fast (~30s) | Slower (~5min) |
| Cross-TU Analysis | Limited | Full |
| Memory Issues | Strong | Good |
| Modern C++ | Basic | Excellent |
| Naming Conventions | No | Yes |
| Fix-it Hints | No | Yes |
| False Positives | More | Fewer |

**Recommended workflow:**
1. Run `cppcheck` for quick feedback during development
2. Run `clang-tidy` before commits for thorough analysis
3. Both tools share `compile_commands.json` for consistent results

## CI/CD Integration

Exit codes:
- `0` - No issues found
- `1` - Warnings found (non-blocking)
- `2` - Errors found (blocking)

### Example GitHub Actions
```yaml
- name: Run clang-tidy
  run: |
    brew install llvm
    export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
    ./tests/clang-tidy/clang_tidy_focused.sh
```

## Results

Output saved to `test_results/`:
- `clang_tidy_full_TIMESTAMP.txt` - Full analysis report
- `clang_tidy_summary_TIMESTAMP.txt` - Summary with counts

## Troubleshooting

### "clang-tidy not found"
```bash
# Verify LLVM installation
ls /opt/homebrew/opt/llvm/bin/clang-tidy

# Add to PATH
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

### "compile_commands.json not found"
```bash
# Generate via CMake
cmake -B build/ -G Ninja -DCMAKE_BUILD_TYPE=Debug
# File is auto-copied to project root by CMakeLists.txt
```

### Too many warnings
1. Review disabled checks in `.clang-tidy`
2. Add project-specific exclusions as needed
3. Use `// NOLINT` comments for intentional patterns:
   ```cpp
   int magic = 42; // NOLINT(readability-magic-numbers)
   ```

## File Organization

```
tests/clang-tidy/
├── .clang-tidy              # Configuration file
├── clang_tidy_focused.sh    # Quick analysis (changed files)
├── run_clang_tidy.sh        # Full analysis (all files)
└── README.md                # This documentation
```
