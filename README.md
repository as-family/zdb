# Distributed DB

Z for Zoza!

## Building

This project uses [vcpkg](https://github.com/Microsoft/vcpkg) for dependency management.

### Initial Setup

1. Run the setup script to install vcpkg:
   ```bash
   ./setup-vcpkg.sh
   ```

### Building the Project

You can use any of the provided CMake presets:

```bash
# Using GCC 13
cmake --preset=gcc-13-preset
cmake --build --preset=gcc-13

# Using LLVM 21
cmake --preset=llvm-21-preset
cmake --build --preset=llvm-21

# Using GCC 16
cmake --preset=gcc-16-preset
cmake --build --preset=gcc-16
```

### Running Tests

```bash
# Configure with testing enabled (default)
cmake --preset=gcc-16-preset

# Build tests
cmake --build --preset=gcc-16

# Run tests
ctest --preset=Test
```

### Dependencies

This project uses the following dependencies managed by vcpkg:
- **gRPC** - For RPC communication
- **spdlog** - For logging
- **Google Test** - For unit testing
