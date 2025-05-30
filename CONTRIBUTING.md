# Contributing to Emergency Drone Coordination System

## Development Workflow

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Make your changes and test thoroughly
4. Ensure all CI checks pass
5. Submit a pull request

## Building and Testing

```bash
# Build all components
make all

# Run tests
make test_list
make test_sdl
make test_throughput

# Check for memory leaks
make valgrind_main
make valgrind_list
make valgrind_throughput
```

## Code Quality Checks

```bash
# Format code (after installing clang-format)
clang-format -i *.c headers/*.h

# Static analysis with cppcheck
sudo apt install cppcheck
cppcheck --enable=all --inconclusive *.c headers/*.h

# Check for security issues
grep -r "strcpy\|strcat\|sprintf\|gets" *.c || echo "No unsafe functions found"
```

## Code Style and Standards

### Documentation Requirements

- Follow the Doxygen documentation format used throughout the codebase
- Use comprehensive file headers like those in [`server_throughput.h`](headers/server_throughput.h):

```c
/**
 * @file filename.h
 * @author Amar Daskin - Wilmer Cuevas - Jelsin Sanchez
 * @brief Brief description of the module
 * @version 0.1
 * @date 2025-05-22
 *
 * Detailed description of the module's purpose and functionality.
 *
 * **Key Features:**
 * - Feature 1 description
 * - Feature 2 description
 *
 * @copyright Copyright (c) 2024
 *
 * @ingroup module_group
 */
```

- Document all public functions with parameter descriptions and return values
- Include `@pre` and `@post` conditions for complex functions
- Use `@note`, `@warning`, and `@see` tags appropriately

### Header Organization

Follow the pattern established in headers like [`view.h`](headers/view.h) and [`ai.h`](headers/ai.h):

```c
/**
 * @defgroup group_name Group Description
 * @brief Brief group description
 * @ingroup parent_group
 * @{
 */

// Group content here

/** @} */ // end of group_name
```

### Thread Safety Documentation

All thread-safe functions must document their synchronization requirements, following the pattern in [`list.h`](headers/list.h):

```c
/**
 * @brief Function description
 *
 * **Thread Safety:**
 * - Mutex protection details
 * - Locking order requirements
 * - Race condition prevention
 *
 * @param param Description
 * @return Return value description
 *
 * @note Thread safety notes
 * @warning Synchronization warnings
 */
```

### Code Formatting

- Follow the existing code style defined in [`.clang-format`](.clang-format)
- Use meaningful variable and function names
- Ensure thread safety in all concurrent operations
- Use the established naming conventions:
  - Functions: `snake_case` (e.g., [`init_perf_monitor`](headers/server_throughput.h))
  - Structures: `PascalCase` (e.g., [`PerfMetrics`](headers/server_throughput.h))
  - Constants: `UPPER_CASE` (e.g., `MAX_SURVIVORS`)

## Architecture Compliance

### Module Integration

New modules must integrate with the existing architecture:

1. **Core Modules**: Follow the pattern of [`controller.c`](controller.c), [`drone.c`](drone.c), [`ai.c`](ai.c)
2. **Data Structures**: Use thread-safe lists from [`list.c`](list.c) and coordinate system from [`coord.h`](headers/coord.h)
3. **Visualization**: Integrate with SDL system from [`view.c`](view.c)
4. **Performance**: Include monitoring hooks from [`server_throughput.c`](server_throughput.c)

### Thread Safety Requirements

All new code must maintain the thread safety standards established in the codebase:

- Use the global mutex patterns from [`survivor.h`](headers/survivor.h) and [`globals.h`](headers/globals.h)
- Follow the locking hierarchy to prevent deadlocks
- Implement proper cleanup in all error paths
- Use semaphores for flow control as shown in [`list.c`](list.c)

## Testing Requirements

### Unit Tests

- All new functions should have corresponding tests
- Test both success and failure cases
- Verify thread safety for concurrent operations
- Follow the existing test patterns in the `tests/` directory

### Integration Tests

- Test client-server communication using the protocol in [`communication-protocol.md`](communication-protocol.md)
- Verify multi-drone coordination
- Test performance under load using [`server_throughput_test.c`](tests/server_throughput_test.c)

### Performance Testing

New features must maintain the performance standards:

- Sub-second mission assignment times
- Support 50+ concurrent drone connections
- Maintain real-time visualization (10 FPS minimum)
- Include performance monitoring using [`server_throughput.h`](headers/server_throughput.h) functions

### Memory Testing

- All code must pass Valgrind checks
- No memory leaks allowed
- Proper cleanup of all resources
- Use the established patterns from [`list.c`](list.c) for memory management

## Network Protocol Compliance

Changes to network communication must follow the established JSON protocol:

```c
// Example from clientDrone.c
struct json_object *drone_info = json_object_new_object();
json_object_object_add(drone_info, "type", json_object_new_string("HANDSHAKE"));
json_object_object_add(drone_info, "drone_id", json_object_new_int(my_drone.id));
```

Refer to [`communication-protocol.md`](communication-protocol.md) for complete message specifications.

## Performance Guidelines

### Metrics Integration

All new features should integrate with the performance monitoring system:

```c
// Record performance metrics
perf_record_status_update(bytes_received);
perf_record_mission_assigned(bytes_sent);
perf_record_response_time(response_time_ms);
```

### Resource Management

- Follow the initialization patterns from [`controller.c`](controller.c)
- Use proper cleanup procedures as shown in [`survivor.c`](survivor.c)
- Implement graceful shutdown handling

## Documentation Standards

### API Documentation

Generate comprehensive documentation using Doxygen:

```bash
# Generate documentation
doxygen Doxyfile

# View HTML documentation
open docs/html/index.html
```

### Code Comments

Follow the commenting style established in the codebase:

```c
/**
 * @brief Brief description
 *
 * Detailed description with multiple paragraphs if needed.
 *
 * **Implementation Details:**
 * - Detail 1
 * - Detail 2
 *
 * @param param1 Description of parameter
 * @param param2 Description of parameter
 * @return Description of return value
 *
 * @pre Preconditions
 * @post Postconditions
 *
 * @note Important notes
 * @warning Critical warnings
 * @see Related functions
 */
```

## Continuous Integration

The project uses GitHub Actions for CI with the following checks:

### Build Matrix

- **Compilers**: GCC and Clang
- **Build Types**: Debug and Release
- **Platforms**: Ubuntu and macOS

### Automated Tests

- ✅ Unit tests ([`tests/listtest.c`](tests/listtest.c), [`tests/sdltest.c`](tests/sdltest.c))
- ✅ Integration tests ([`tests/multi_drone_test.c`](tests/multi_drone_test.c))
- ✅ Performance tests ([`tests/server_throughput_test.c`](tests/server_throughput_test.c))
- ✅ Memory leak detection with Valgrind
- ✅ Static analysis with cppcheck
- ✅ Code formatting verification
- ✅ Security vulnerability scanning

## Pull Request Guidelines

### Before Submitting

1. ✅ All tests pass locally
2. ✅ Code follows the documentation standards shown in headers like [`server_throughput.h`](headers/server_throughput.h)
3. ✅ Thread safety documented and implemented properly
4. ✅ Performance metrics integrated where applicable
5. ✅ No memory leaks detected with Valgrind
6. ✅ Doxygen documentation generated successfully

### PR Description Template

```markdown
## Summary

Brief description of changes

## Changes Made

- [ ] Feature implementation following established patterns
- [ ] Thread safety measures added/updated
- [ ] Performance monitoring integration
- [ ] Documentation updated

## Architecture Compliance

- [ ] Follows module patterns from existing code
- [ ] Integrates with global data structures
- [ ] Maintains thread safety standards
- [ ] Includes proper error handling

## Testing

- [ ] Unit tests added/updated
- [ ] Integration tests pass
- [ ] Performance impact assessed
- [ ] Memory leak testing completed

## Documentation

- [ ] Doxygen comments added
- [ ] Header file properly organized
- [ ] Architecture documentation updated

## Breaking Changes

List any breaking changes and migration path

## Related Issues

Closes #123
```

### Review Process

- At least one maintainer approval required
- All CI checks must pass
- Performance regression tests must pass
- Documentation must be updated for user-facing changes
- Code must follow the established patterns in the codebase

## Project Structure Reference

```
Emergency-Drone-Coordination/
├── controller.c              # Main system orchestration following established patterns
├── clientDrone.c            # Client drone implementation with network protocol
├── drone.c                  # Server-side drone management with thread safety
├── ai.c                     # Mission assignment AI with performance monitoring
├── list.c                   # Thread-safe data structures with semaphore protection
├── map.c                    # Spatial grid management with coordinate system
├── survivor.c               # Emergency scenario simulation with lifecycle management
├── view.c                   # SDL visualization with real-time rendering
├── server_throughput.c      # Performance monitoring with comprehensive metrics
├── headers/                 # Header files with Doxygen documentation
│   ├── server_throughput.h  # Performance monitoring system definitions
│   ├── view.h              # SDL visualization system definitions
│   ├── ai.h                # AI algorithm system definitions
│   ├── drone.h             # Drone management system definitions
│   ├── list.h              # Thread-safe data structure definitions
│   ├── survivor.h          # Survivor management system definitions
│   ├── coord.h             # Coordinate system definitions
│   ├── map.h               # Spatial grid system definitions
│   └── globals.h           # Global variables and system constants
├── tests/                   # Test programs with comprehensive coverage
├── .github/workflows/       # CI configuration with multi-platform testing
└── documentation files      # Project documentation and protocols
```

## Getting Help

- 📖 Read the comprehensive code documentation in the headers
- 🔍 Study existing implementations like [`server_throughput.c`](server_throughput.c) for patterns
- 🐛 Report bugs using [GitHub Issues](../../issues)
- 💬 Ask questions in [Discussions](../../discussions)
- 📧 Contact maintainers for urgent issues

## Performance Standards

All contributions must maintain the established performance standards:

- **Message throughput**: Support for high-frequency drone communication
- **Response times**: Sub-second mission assignment as shown in [`ai.c`](ai.c)
- **Memory efficiency**: Contiguous allocation patterns from [`list.c`](list.c)
- **Thread safety**: Comprehensive synchronization as in [`drone.c`](drone.c)
- **Monitoring integration**: Performance tracking using [`server_throughput.h`](headers/server_throughput.h)

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
