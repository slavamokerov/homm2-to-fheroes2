# Contributing to homm2-to-fheroes2

Thanks for your interest in contributing! The project is deliberately
focused, and its process is lightweight.

## Project vision

The converter does exactly one thing: it turns saves made by the
original **Heroes of Might and Magic II** into saves for the
[fheroes2](https://github.com/ihhub/fheroes2) engine. It is
**one-way on purpose** — there is no fheroes2 → original conversion and
there never will be. The tool is self-contained (no Qt, no fheroes2
sources, no game data files).

Feature requests should keep this scope in mind. If you need something
different (e.g. editing an fheroes2 save), check the
[fheroes2-save-editor](https://github.com/slavamokerov/fheroes2-save-editor)
project — solving the same problem twice is out of scope.

## How contributions are accepted

- Open an issue first for anything bigger than a typo; describe the
  problem and, ideally, attach an affected save (they are small and help
  enormously).
- Pull requests must pass the test suite (see below) and follow the code
  conventions listed here.
- One concern per pull request; unrelated changes belong to separate PRs.
- You can expect a response from a maintainer within a few days. If you
  hear nothing for two weeks, feel free to ping the thread.
- If a contribution does not fit the vision it will be closed with an
  explanation — that is not a verdict on the quality of your work.
  The project is maintained in spare time, so small and well-described
  changes get in much faster.

## Setup

Native CLI (Dependencies: CMake ≥ 3.16, a C++17 compiler, zlib):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Web version (requires the [Emscripten SDK](https://emscripten.org/) on
PATH):

```bash
./web/build.sh        # output in web/deploy/
```

## Tests

All code changes must pass:

```bash
cmake --build build && ctest --test-dir build
```

The suite converts every fixture save (in all three format versions) and
validates the result structure, plus compares the converted Slugfest map
tile-by-tile against a reference save made by fheroes2 itself. New
conversion rules should get a regression check where practical.

## Code conventions

- C++17, no Qt, no external dependencies beyond zlib.
- Comments in code are written in English.
- Keep the existing style: 4-space indentation, braces on new lines,
  `type variableName`, namespaces `h2` (the original format) and `fh2`
  (the fheroes2 format).
- The docs (`README.md`, `docs/*`) are in English. Links between
  documentation sections use markdown anchors, not bare section numbers.
- Original-game saves (`tests/fixtures/*.GM1/.GMC/.GXC`) are testing
  fixtures only — do not commit new ones from games you don't own.

## Where things live

- `src/homm2_save.*` — parser of the original save format (layout
  reference: `docs/HOMM2_SAVE_FORMAT.md`).
- `src/fheroes2_save.*` — writer of the fheroes2 save format.
- `src/convert.*` — the conversion logic and mapping tables.
- `web/` — the browser front end (Emscripten/embind + plain JS).
- `tools/` — developer helpers (`analyze.py` — summary of a save).

## Code of Conduct

This project follows the
[Contributor Covenant](https://www.contributor-covenant.org/version/2/1/code_of_conduct/)
— see [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Report unacceptable
behavior privately to the maintainer.
