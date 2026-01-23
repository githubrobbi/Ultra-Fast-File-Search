# Ultra Fast File Search - Documentation

**Version**: 2026-01-23  
**Status**: Active Development

This documentation is comprehensive enough that a competent developer could reimplement the entire system from scratch.

---

## Quick Links

| I want to... | Go to... |
|--------------|----------|
| Understand the system architecture | [Architecture Overview](architecture/01-overview.md) |
| Learn how MFT reading works | [MFT Reading Deep Dive](architecture/02-mft-reading.md) |
| Build the project | [Build System](architecture/09-build-system.md) |
| Use the CLI | [CLI Reference](reference/cli-reference.md) |
| Understand Rust vs C++ performance | [Rust Implementation Comparison](rust/implementation-comparison.md) |

---

## Documentation Structure

### 📐 Architecture (Core Design)

The numbered architecture documents explain how UFFS works internally. Read these in order for a complete understanding.

| # | Document | Description |
|---|----------|-------------|
| 01 | [Overview](architecture/01-overview.md) | High-level system design, components, data flow |
| 02 | [MFT Reading](architecture/02-mft-reading.md) | NTFS MFT structure, reading strategies, I/O patterns |
| 03 | [Concurrency](architecture/03-concurrency.md) | Threading model, IOCP, synchronization |
| 04 | [MFT Parsing](architecture/04-mft-parsing.md) | Record parsing, attribute extraction, fixups |
| 05 | [Pattern Matching](architecture/05-pattern-matching.md) | Glob, regex, Unicode handling |
| 06 | [GUI](architecture/06-gui.md) | Virtual ListView, owner-draw, shell integration |
| 07 | [Indexing](architecture/07-indexing.md) | In-memory index, path resolution |
| 08 | [Search](architecture/08-search.md) | Query execution, filtering, ranking |
| 09 | [Build System](architecture/09-build-system.md) | Compilation, dependencies, configuration |
| 10 | [Performance](architecture/10-performance.md) | Benchmarking, profiling, optimization |
| 11 | [Future Optimizations](architecture/11-future-optimizations.md) | Potential performance improvements |

### 📚 Reference

Technical reference documentation for specific features and tools.

| Document | Description |
|----------|-------------|
| [CLI Reference](reference/cli-reference.md) | Command-line options and usage |
| [NTFS Metafiles](reference/ntfs-metafiles.md) | System files ($MFT, $Bitmap, etc.) |
| [MFT Benchmark Tool](reference/mft-benchmark-tool.md) | Performance measurement tools |
| [MFT Concurrency Model](reference/mft-concurrency-model.md) | Detailed async I/O architecture |

### 🦀 Rust Implementation

Documentation for the Rust MFT reader implementation and comparison with C++.

| Document | Description |
|----------|-------------|
| [Implementation Comparison](rust/implementation-comparison.md) | Rust vs C++ performance analysis |
| [I/O Optimization Plan](rust/io-optimization-plan.md) | Rust I/O improvement roadmap |

### 📝 Design Proposals

Design documents for proposed features.

| Document | Description |
|----------|-------------|
| [Streaming Search](design/streaming-search.md) | Progressive result display design |

### 🔄 Migration Guides

Guides for upgrading dependencies and migrating code.

| Document | Description |
|----------|-------------|
| [LLVM to CLI11](migration/llvm-to-cli11.md) | Command-line parser migration |
| [Boost Upgrade](migration/boost-upgrade.md) | Boost 1.73 → 1.90 upgrade plan |

---

## Documentation Standards

### Target Audience

**Primary:** Senior C++ developers who want to:
- Understand exactly how UFFS achieves its performance
- Contribute to the codebase
- Build similar tools for other platforms

**Prerequisites assumed:**
- Proficiency in C++ (templates, RAII, move semantics)
- Basic understanding of operating systems
- Familiarity with Windows development

### Level of Detail

Each document provides:
1. **Complete implementation details** - Not just "what" but exactly "how"
2. **Rationale for design decisions** - Why this approach vs alternatives
3. **Edge cases and error handling** - Real-world robustness
4. **Verification** - How to confirm correct implementation

---

## Contributing

When adding or updating documentation:

1. Follow the existing structure and naming conventions
2. Include code examples from actual source (not invented)
3. Add diagrams for complex architectures (ASCII art preferred)
4. Cross-reference related documents
5. Update this README if adding new documents

---

## Version History

| Date | Change |
|------|--------|
| 2026-01-23 | Reorganized documentation structure |
| 2026-01-23 | Added Rust implementation comparison docs |
| 2026-01-23 | Added future optimizations roadmap |

