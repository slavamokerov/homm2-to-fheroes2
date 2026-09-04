# Debug builds of fheroes2 with save-load diagnostics

`fheroes2-1.1.17-debug-logging.patch` adds diagnostics to fheroes2 1.1.17
(the version used for save-format verification) so that every save-load
failure can be located precisely:

- `COUT` for macOS app bundles goes to stderr instead of syslog
  (`src/engine/logging.h`);
- `StreamBase::setFail()` prints a backtrace (`src/engine/serialize.cpp`);
- `game_io.cpp`, `world.cpp`, `kingdom.cpp`, `castle.cpp`, `heroes.cpp`
  log the remaining byte count after every load section and the exact
  validation that failed (castle tile indices, hero ids, kingdom fields);
- AI diagnostics (`ai_planner_hero.cpp`, `ai_planner.cpp`,
  `ai_hero_action.cpp`): unhandled-object assert logs the object type with
  a backtrace, the AI action-object cache logs add/erase around trouble
  tiles, `HeroesAction`/`HeroesActionComplete` log their tile argument;
- `maps_tiles.cpp`: `Tile::setMainObjectType` logs type changes with a
  backtrace for a chosen tile index (used to trace who removes map
  objects, e.g. AI picking up chests).

The patched sources live in the git worktree at `dev/fheroes2-dbg`
(commit `2685c2188`, fheroes2 1.1.17); the patch file is the source of
truth. To rebuild the debug engine from scratch:

```bash
# 1. Get the sources (any checkout of fheroes2 1.1.17).
git -C ~/Projects/fheroes2-reference worktree add /tmp/fheroes2-dbg 2685c2188

# 2. Apply the diagnostics patch.
git -C /tmp/fheroes2-dbg apply /path/to/fheroes2-1.1.17-debug-logging.patch

# 3. Configure and build (Debug enables WITH_DEBUG; the user's
#    fheroes2.cfg already has debug = 5460).
cmake -S /tmp/fheroes2-dbg -B /tmp/fheroes2-dbg/build \
      -DCMAKE_BUILD_TYPE=Debug -DMACOS_APP_BUNDLE=ON \
      -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build /tmp/fheroes2-dbg/build -j 8

# 4. Link the game data into the app bundle.
R=/tmp/fheroes2-dbg/build/fheroes2.app/Contents/Resources
ln -sfn "$HOME/Library/Application Support/fheroes2/data"  "$R/data"
ln -sfn "$HOME/Library/Application Support/fheroes2/files" "$R/files"
ln -sfn "$HOME/Library/Application Support/fheroes2/maps"  "$R/maps"
ln -sfn "$HOME/Library/Application Support/fheroes2/music" "$R/music"

# 5. Run with stderr captured.
/tmp/fheroes2-dbg/build/fheroes2.app/Contents/MacOS/fheroes2 \
    > /tmp/fheroes2_dbg.log 2>&1 &
```

Assertions of the Debug build print `file:line` to stderr, which is how
the tavern recruits assertion (`kingdom.cpp:490`) and the castle tile
validation were found. See `SESSION_NOTES.md` for the debugging workflow.
