# Security Policy

## Reporting a Vulnerability

homm2-to-fheroes2 parses untrusted binary files (save files), so a crash
or a memory-safety bug in the parser is a real concern worth reporting.

Please **open an issue** in this repository with:

- a description of the problem and its impact;
- the save file or the smallest input that reproduces it (saves are
  small and greatly speed things up);
- the tool version (CLI build or the web converter) and platform.

Do not expect a private disclosure process — the project has no
security mailing list; either use the public tracker or, if the issue is
sensitive, contact the maintainer directly.

## Supported versions

Only the latest release (and `main`) receive fixes.

## Scope

- In scope: crashes, hangs, out-of-bounds reads/writes and incorrect
  output when converting a save file.
- Out of scope: the web page's static hosting, GitHub Pages itself, and
  issues in third-party projects (fheroes2, Emscripten).
