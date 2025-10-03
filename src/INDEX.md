# uSEQ Refactored Architecture - Documentation Index

Welcome to the refactored uSEQ codebase! This index helps you navigate the documentation.

## Start Here

**New to this architecture?** Start with:

1. 📖 **[QUICKSTART.md](QUICKSTART.md)** (10 minutes)
   - Core concepts in 60 seconds
   - Hello World example
   - Common tasks
   - Mental model

2. 📘 **[README.md](README.md)** (20 minutes)
   - Complete architecture overview
   - Directory structure
   - Key components explained
   - Benefits vs legacy code

3. 🗺️ **[ARCHITECTURE.txt](ARCHITECTURE.txt)** (15 minutes)
   - Visual diagrams
   - Data flow walkthrough
   - Design decisions explained
   - Testing strategy

## For Different Audiences

### Developers: "How do I use this?"

1. Start: [QUICKSTART.md](QUICKSTART.md)
2. Study: [examples/simple_mock_test.cpp](examples/simple_mock_test.cpp)
3. Read: [core/useq_state.h](core/useq_state.h)
4. Read: [core/useq_update.h](core/useq_update.h)
5. Experiment: [platforms/mock/mock_hardware.h](platforms/mock/mock_hardware.h)

**Estimated time:** 1-2 hours to get productive

### Maintainers: "How do I migrate legacy code?"

1. Start: [MIGRATION.md](MIGRATION.md)
2. Compare: Legacy `uSEQ/src/uSEQ_update.cpp` vs `src/core/useq_update.h`
3. Study patterns: [MIGRATION.md Common Patterns section](MIGRATION.md#common-patterns)
4. Test approach: [MIGRATION.md Testing Your Migration](MIGRATION.md#testing-your-migration)

**Estimated time:** 2-3 hours to understand migration strategy

### Architects: "What are the design principles?"

1. Start: [PHASE1_SUMMARY.md](PHASE1_SUMMARY.md)
2. Deep dive: [ARCHITECTURE.txt Design Decisions](ARCHITECTURE.txt)
3. Philosophy: [README.md Philosophy section](README.md#philosophy)
4. Future: [PHASE1_SUMMARY.md Phase 2 & 3](PHASE1_SUMMARY.md#next-steps-phase-2)

**Estimated time:** 1 hour for strategic overview

### Hardware Engineers: "How do I add a new platform?"

1. Start: [QUICKSTART.md Task: Add a New Platform](QUICKSTART.md#task-add-a-new-platform)
2. Interface: [core/hardware_config.h](core/hardware_config.h)
3. Example: [platforms/hardware_v1_0/hardware_v1_0_config.h](platforms/hardware_v1_0/hardware_v1_0_config.h)
4. Testing: [platforms/mock/mock_hardware.h](platforms/mock/mock_hardware.h)

**Estimated time:** 30 minutes to understand, 1-2 days to implement

## Documentation Files

### Quick Reference
- **[QUICKSTART.md](QUICKSTART.md)** - Get started in 10 minutes
- **[INDEX.md](INDEX.md)** - This file

### Core Documentation
- **[README.md](README.md)** - Complete architecture guide
- **[ARCHITECTURE.txt](ARCHITECTURE.txt)** - Visual diagrams and data flow
- **[PHASE1_SUMMARY.md](PHASE1_SUMMARY.md)** - What we've accomplished and next steps

### Guides
- **[MIGRATION.md](MIGRATION.md)** - How to migrate legacy code
- **[examples/simple_mock_test.cpp](examples/simple_mock_test.cpp)** - Complete working example

## Code Files

### Core (Platform-Agnostic)
```
core/
├── useq_state.h           # All state in one struct
├── useq_update.h          # Update loop functions
└── hardware_config.h      # Platform interface
```

**Key characteristics:**
- No inheritance
- No virtual functions
- No preprocessor conditionals
- Template-based
- Pure functions + data

### Platforms (Platform-Specific)
```
platforms/
├── mock/
│   └── mock_hardware.h              # Testing platform
├── hardware_v1_0/
│   └── hardware_v1_0_config.h       # uSEQ v1.0 eurorack
├── musicthing/                      # (TODO: Phase 2)
└── wasm/                            # (TODO: Phase 2)
```

**Key characteristics:**
- One config file per platform
- All platform-specific code isolated here
- Implements HardwareConfig interface

### Examples
```
examples/
└── simple_mock_test.cpp    # Complete working example
```

## By Topic

### Understanding the Architecture

| Topic | Primary Resource | Secondary Resources |
|-------|-----------------|---------------------|
| Overview | [README.md](README.md) | [ARCHITECTURE.txt](ARCHITECTURE.txt) |
| Quick concepts | [QUICKSTART.md](QUICKSTART.md) | [examples/simple_mock_test.cpp](examples/simple_mock_test.cpp) |
| Design philosophy | [README.md Philosophy](README.md#philosophy) | [ARCHITECTURE.txt Design Decisions](ARCHITECTURE.txt) |
| Data structures | [core/useq_state.h](core/useq_state.h) | [QUICKSTART.md Core Concepts](QUICKSTART.md#core-concepts-60-seconds) |
| Update loop | [core/useq_update.h](core/useq_update.h) | [ARCHITECTURE.txt Data Flow](ARCHITECTURE.txt) |
| Platform abstraction | [core/hardware_config.h](core/hardware_config.h) | [platforms/mock/mock_hardware.h](platforms/mock/mock_hardware.h) |

### Practical Tasks

| Task | Primary Resource | Secondary Resources |
|------|-----------------|---------------------|
| Get started | [QUICKSTART.md](QUICKSTART.md) | [examples/simple_mock_test.cpp](examples/simple_mock_test.cpp) |
| Add new platform | [QUICKSTART.md Task: Add Platform](QUICKSTART.md#task-add-a-new-platform) | [platforms/hardware_v1_0/](platforms/hardware_v1_0/hardware_v1_0_config.h) |
| Migrate legacy code | [MIGRATION.md](MIGRATION.md) | [PHASE1_SUMMARY.md](PHASE1_SUMMARY.md) |
| Write tests | [README.md Testing](README.md#testing) | [platforms/mock/mock_hardware.h](platforms/mock/mock_hardware.h) |
| Understand data flow | [ARCHITECTURE.txt Data Flow](ARCHITECTURE.txt) | [core/useq_update.h](core/useq_update.h) |

### Comparisons

| Comparison | Resource |
|-----------|----------|
| Legacy vs Refactored | [README.md Benefits](README.md#benefits-vs-legacy-code) |
| Old way vs New way | [QUICKSTART.md Core Concepts](QUICKSTART.md#core-concepts-60-seconds) |
| Before/After patterns | [MIGRATION.md Step-by-Step](MIGRATION.md#step-by-step-migration) |
| Design decisions | [ARCHITECTURE.txt Comparison](ARCHITECTURE.txt) |

## Recommended Reading Paths

### Path 1: "I want to understand quickly" (30 minutes)
1. [QUICKSTART.md](QUICKSTART.md) - 10 min
2. [examples/simple_mock_test.cpp](examples/simple_mock_test.cpp) - 10 min
3. [ARCHITECTURE.txt](ARCHITECTURE.txt) overview sections - 10 min

### Path 2: "I want to add a feature" (1 hour)
1. [QUICKSTART.md](QUICKSTART.md) - 10 min
2. [core/useq_state.h](core/useq_state.h) - 15 min
3. [core/useq_update.h](core/useq_update.h) - 20 min
4. [platforms/mock/mock_hardware.h](platforms/mock/mock_hardware.h) - 15 min

### Path 3: "I want to add a platform" (2 hours)
1. [QUICKSTART.md Task: Add Platform](QUICKSTART.md#task-add-a-new-platform) - 10 min
2. [core/hardware_config.h](core/hardware_config.h) - 20 min
3. [platforms/mock/mock_hardware.h](platforms/mock/mock_hardware.h) - 30 min
4. [platforms/hardware_v1_0/hardware_v1_0_config.h](platforms/hardware_v1_0/hardware_v1_0_config.h) - 40 min
5. [README.md Future Platforms](README.md#future-platforms) - 20 min

### Path 4: "I want to migrate code" (3 hours)
1. [MIGRATION.md](MIGRATION.md) - 1 hour
2. Compare legacy vs refactored files - 1 hour
3. [PHASE1_SUMMARY.md](PHASE1_SUMMARY.md) - 30 min
4. Practice with small example - 30 min

### Path 5: "I want the complete picture" (4 hours)
1. [QUICKSTART.md](QUICKSTART.md) - 10 min
2. [README.md](README.md) - 30 min
3. [ARCHITECTURE.txt](ARCHITECTURE.txt) - 30 min
4. [PHASE1_SUMMARY.md](PHASE1_SUMMARY.md) - 30 min
5. [MIGRATION.md](MIGRATION.md) - 1 hour
6. Study code files - 1 hour
7. Try example - 20 min

## Key Concepts Reference

### Data Structures
- **USEQState** - All program state ([core/useq_state.h](core/useq_state.h))
- **HardwareConfig** - Platform interface ([core/hardware_config.h](core/hardware_config.h))
- **Value, Environment, Parser** - LISP components (unchanged from legacy)

### Core Functions
- **useq_tick()** - Main update loop
- **useq_update_signals()** - Evaluate expressions
- **useq_update_outs()** - Write to hardware
- **useq_update_inputs()** - Read from hardware

All in [core/useq_update.h](core/useq_update.h)

### Design Patterns
- **Composition over Inheritance** - `USEQState` contains interpreter
- **Templates over Virtuals** - Compile-time polymorphism
- **Explicit over Implicit** - Pass state as parameter
- **Data over Code** - Configuration in structs, not preprocessor

Explained in [ARCHITECTURE.txt](ARCHITECTURE.txt)

## Frequently Asked Questions

**Q: Where should I start?**
A: [QUICKSTART.md](QUICKSTART.md) → [examples/simple_mock_test.cpp](examples/simple_mock_test.cpp) → [README.md](README.md)

**Q: How is this different from the legacy code?**
A: See [README.md Benefits vs Legacy](README.md#benefits-vs-legacy-code) and [ARCHITECTURE.txt Comparison](ARCHITECTURE.txt)

**Q: How do I migrate my code?**
A: [MIGRATION.md](MIGRATION.md) has step-by-step patterns

**Q: How do I add a new platform?**
A: [QUICKSTART.md Task: Add Platform](QUICKSTART.md#task-add-a-new-platform) + [core/hardware_config.h](core/hardware_config.h)

**Q: Is this faster or slower than legacy?**
A: Same speed or faster! Templates have zero overhead. See [QUICKSTART.md Q&A](QUICKSTART.md#q-is-this-slower-than-the-old-code)

**Q: Can I still use the old code?**
A: Yes! This lives in `src/`, old code in `uSEQ/src/`. Both work.

**Q: What's next after Phase 1?**
A: [PHASE1_SUMMARY.md Next Steps](PHASE1_SUMMARY.md#next-steps-phase-2)

## Contributing

When adding to the refactored architecture:

1. **Core logic** → `src/core/` (no platform-specific code!)
2. **Platform code** → `src/platforms/your_platform/`
3. **Examples** → `src/examples/`
4. **Documentation** → Update this INDEX.md

## Version History

- **Phase 1** (Current): Core architecture established
  - [PHASE1_SUMMARY.md](PHASE1_SUMMARY.md) for details
- **Phase 2** (Planned): Split large API files, convert first platform
- **Phase 3** (Future): Complete migration, remove legacy code

## Getting Help

1. Check this INDEX.md for relevant documentation
2. Read the [QUICKSTART.md](QUICKSTART.md) FAQ
3. Study [examples/simple_mock_test.cpp](examples/simple_mock_test.cpp)
4. Compare legacy code with refactored in [MIGRATION.md](MIGRATION.md)

## Summary

This refactored architecture provides:
- ✅ **Simplicity** - Data + functions, no inheritance
- ✅ **Clarity** - Explicit state and platform separation
- ✅ **Testability** - Mock hardware for unit tests
- ✅ **Extensibility** - New platform = new config file
- ✅ **Performance** - Zero overhead abstractions
- ✅ **Maintainability** - No preprocessor in core logic

Start with [QUICKSTART.md](QUICKSTART.md) and explore from there!
