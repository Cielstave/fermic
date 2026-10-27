# Fermi

Fermi is a systems-oriented language and toolchain for building high-performance backend services, runtime components, and infrastructure software with explicit ownership and memory safety in mind.

## Overview

Fermi combines a focused language design with a native compiler pipeline and runtime model intended for robust systems programming. The project emphasizes:

- explicit resource ownership and borrowing semantics
- predictable memory and runtime behavior
- native code generation with validation-oriented tooling
- a document-first development model for language contracts and architecture

## Current status

Fermi is under active development. The compiler, runtime, and language specification are evolving together, and the project is currently positioned as an experimental systems language rather than a production-ready general-purpose platform.

The repository includes source code, specification documents, examples, and tooling for:

- compiler and frontend work
- runtime and memory model support
- protocol and system-oriented examples
- design and validation documentation

## Documentation

For deeper context, architecture, and project goals, see:

- [docs/README.md](docs/README.md)
- [docs/PLAN.md](docs/PLAN.md)
- [docs/spec/README.md](docs/spec/README.md)

## Project structure

- `compiler/` — compiler and language pipeline components
- `runtime/` — runtime support and execution infrastructure
- `examples/` — runnable examples and feature demonstrations
- `tests/` — test and validation assets
- `docs/` — architecture, roadmap, and language specification
- `bootstrap/` — bootstrap/frontend experiments
- `dispatch/` — workload simulation and runtime-oriented examples

## Build and validation

Typical local workflow:

```sh
make
make test
```

Additional validation commands and tooling are documented in the project docs and include sanitizer, runtime, and benchmark-oriented workflows.

## Mission

Fermi aims to provide a practical, auditable path for building native software with stronger safety guarantees than traditional unchecked systems programming, while remaining aligned with the needs of performance-sensitive backend and infrastructure workloads.

## License

Repository licensing details should be checked in the project root and relevant policy files before redistribution or contribution.
