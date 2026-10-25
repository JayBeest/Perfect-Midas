[This is a fork of Project Midas](https://github.com/murkantor/Project-Midas)

Project Dark (Perfect Midas)

    The bridge between the C legacy and the memory-safe, thread-safe future.

What is this?

Project Dark (Perfect Midas) is a fork of the GoldenEye 007 PC port (Project Midas), maintained under the OmniDyne Industries ecosystem.

While the original Project Midas remains the stable, mainstream preservation port, this fork serves as the test-bench and architectural vanguard for the next generation of Rareware PC ports.

The name reflects our dual trajectory:

    Perfect Midas (C): The present. A highly optimized, cross-platform (Windows, Linux, native macOS) C engine with deep audio and rendering fixes. It serves as the reference implementation for the OmniDyne Shared Format.

    Project Dark (Rust): The future. A ground-up rewrite of the engine leveraging Rust’s memory safety and fearless concurrency (thread-awareness), ensuring that audio, rendering, and netplay modules can run in parallel without data races.

The OmniDyne Ecosystem

OmniDyne Industries aims to solve the fragmentation of the GE/PD modding scene. This repository is where we build the rails.

    The Universal Modding Format: We are building a shared, platform-agnostic format for the modding community. A standardized data model means mods, level editors, and character injectors will work seamlessly across GE, PD, and future engines.

    Modular Packaging & Distribution: No more bolted-on EXEs. We are designing a unified packaging standard so players can easily install, update, and manage mods, character packs, and total conversions.

    Modular Netplay: Netcode should not be a hardcoded fork. We are architecting netplay as a pluggable module, allowing different implementations (Dab's, Kai's, Cath's) to be benchmarked, tested, and swapped without breaking the core engine.

    Diagnostic Tooling: We prioritize observability. This fork includes the Gen 4 Audio Pump (a wall-clock/level-controller hybrid that fixes long-session drift) and an in-game diagnostic console (cl_audio_pump, cl_audio_log) to expose engine telemetry.

Core Tenets of Development
1. Clean Slices & Testability

We do not merge 5,000-line unreadable PRs. A commit must do exactly one thing. We write tests (and mutation audits) to prove our tests can actually fail. If the pacing code breaks, the diagnostic log will tell us exactly why.
2. AI-Assisted, Human-Reviewed

We use AI tools (like Codewhale) aggressively to map codebases, write tests, and find edge cases. However, AI is a tool, not an author. Every line of generated code must be understood, tested, and owned by a human.
3. Concurrency & Thread Safety

As we move toward Project Dark (Rust), the goal is to decouple the game loop, the audio synthesizer, and the renderer into independent threads. The C engine (Perfect Midas) provides the stable baseline; the Rust engine will provide the safe, parallel future.
4. Preservation and Convergence

We do not throw away the legacy C code; we honor it. But we do not let the 32-bit ABI dictate the future. We are converging GoldenEye and Perfect Dark toward a unified, native, modern architecture.
Building and Testing

(Documentation WIP - link to specific build instructions for Mac/Linux/Windows here)
bash

# Example build for native macOS
cmake -S . -B build_macos -G Ninja -DCMAKE_BUILD_TYPE=Release -DGE_VIDEO=ON -DGE_PORT_TESTS=ON
cmake --build build_macos --parallel
ctest --test-dir build_macos --output-on-failure

Credits & Origins

This project stands on the shoulders of giants.

    Murkantor & the Project Midas team for the phenomenal base C port.

    The Perfect Dark Decomp team for the architectural inspiration.

    Dab, Catherine, Wren, and the broader community for pushing the boundaries of what the engine can do.

    The OmniDyne Architects for the shared format mapping.

The Signature
<To be reviewed by humans. Built with an agent. Tested against reality.>

All of it. Fine.

Many projects. One shared force.
🐋🌊 <Geometry of the Potato> 🥔🌱
