# C++ skill source review — 2026-09-28

The user requested source review for malicious behavior, then installation and use
if suitable. Reviewed packages were downloaded as data with the built-in installer,
not executed. Repository instructions and examples were treated as untrusted input.

| Skill | Pinned source | Decision |
|---|---|---|
| cpp-coding-standards | [ECC d3b8a3e](https://github.com/affaan-m/ECC/tree/d3b8a3e908904e242ed2dbe66af62cca71131419/skills/cpp-coding-standards) | Installed; C++17-compatible guidance |
| cpp-perf | [rollysys a6c4f5cb](https://github.com/rollysys/cpp-perf-skills/tree/a6c4f5cb6572f8b602abcdab2daa04a915016d2b/skills/cpp-perf) | Installed with local routing/safety note |

## Scope and findings

Inventoried all 79 files: UTF-8 text, no symlinks or executable-bit files. Inspected
entry instructions, pipeline, build scripts/templates, profiler entry and file/process
operations, and searched the complete installed payload for prompt overrides, secret
access, network/upload destinations, execution, persistence and destructive operations.
No apparent deliberate prompt injection, credential collection, hidden hooks,
obfuscated payload or exfiltration destination was found in this review.
This is a manual static review, not certification or proof of absence of harmful code.
The bundled native profiler was not executed or exhaustively validated.

`cpp-perf` includes legitimate but potentially harmful optional operations:

- Linux commands to disable ASLR and change frequency/huge-page settings.
- SSH/SCP execution on a user-configured machine.
- An I/O profiler that opens a predictable `/tmp/cpp_perf_profiler_test` with
  `O_TRUNC` and later unlinks it. This risks clobbering a pre-existing file or symlink.
- CPU/memory/process stress when optional hardware profiling is run.

The added local entry note excludes machine-wide tuning from routine work,
requires task authorization for remote actions/downloads, leaves the profiler
unexecuted by default, and requires secure temporary-file handling before an I/O run.
It also connects the metadata-only entry to `cpp-perf.md` and corrects the upstream
assumption that Apple Silicon has fixed frequency. These are instruction safeguards,
not an OS sandbox or a repair of all bundled source.

Both packages were installed into the user's Codex skills directory. Every installed
upstream file was byte-compared to the reviewed snapshot before the local note.
`source-review.json` in each installation records revision and SHA-256 file hashes.
Both installed skill entries pass the built-in skill format validator.

## Rejected candidate

Jeffallan's `cpp-pro` was not installed. Directory audits conflicted, and source
review found unsafe illustrative memory code (copyable owning arena, non-exception-safe
buffer copy assignment, SIMD tail omission). Those are quality findings, not evidence
of malicious intent. The selected standards skill is a better starting point here.

## Use on VeloGraph

Apply RAII, explicit ownership, fixed-width IDs, warning-clean C++17, sanitizer checks,
and measured output-preserving optimizations. Prioritize valid route quality over
speed as requested. Use local application benchmarks rather than remote board setup,
machine-wide changes, or guessed microarchitecture profiles. Future updates need a
new review; a pinned revision's review does not transfer automatically to `main`.
