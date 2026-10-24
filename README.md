# MassSprings - Puzzle Board State Space Explorer

All combinations of pieces reachable from an initial puzzle are explored, the resulting puzzle state-space is visualized as a force-directed graph.
The graph layout is calculated iteratively using a mass-spring-system with additional pairwise repulsive forces simulated using Barnes-Hut.

![](screenshot.png)

## Running

Requirements for a locally built binary:

- Directory `fonts`
- Directory `shader`
- Preset file `default.puzzle` (optional)
- OpenGL 4.3 capable GPU and driver (raylib is built with `GRAPHICS_API_OPENGL_43`)

The GUI needs these. The headless `--space` and `--state-space` modes do not open a window and do not need the fonts, shaders or an OpenGL context.

Run the application with:

```sh
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings
```

The Nix wrapper bundles `fonts/`, `shader/` and `default.puzzle`, so it can be started from any directory. A locally built binary has to be started from a directory containing the listed requirements. Program arguments go after `--`:

```sh
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --help
```

### Command line arguments

| Argument | Default | Description |
| --- | --- | --- |
| `<preset-file>` / `--presets <file>` | `default.puzzle` | Preset file for the GUI or a headless mode |
| `--preset <index>` | `0` in headless modes | GUI: start with this preset; `--space`/`--state-space`: use this preset (0-based index) |
| `--puzzle <type>` | – | GUI: start with a puzzle type; `--state-space`: first preset of that type (`sliding_block`, `lights_out`, `hanoi`, `eight_puzzle` or `sudoku4`) |
| `--populate` | – | Populate the graph for the selected preset at startup (GUI only) |
| `--space <preset\|rh\|klotski>` | – | Generate a puzzle space headlessly instead of opening the GUI |
| `--state-space` | – | Explore a preset's state space headlessly and print benchmark statistics |
| `--output <file>` | `puzzle_space.puzzles` | Output file for generated puzzle spaces (`MSPACE` format, `--space` only) |
| `--blocks <0..15>` | `5` | Maximum number of blocks per generated puzzle; `0` uses all movable blocks on the selected board (preset mode) |
| `--min-blocks <1..15>` | `2` | Minimum number of blocks per generated puzzle, target included (`1` behaves like `2`) |
| `--wall-inventory` | off | Use the current board's wall shapes as an inventory instead of fixed walls (`--space preset` only) |
| `--min-walls <0..N>` | `0` | Minimum wall pieces to generate when `--wall-inventory` is enabled |
| `--max-walls <0..N>` | current wall pieces | Maximum wall pieces to generate when `--wall-inventory` is enabled |
| `--moves <int>` | `10` | Minimum required moves for a cluster to be saved (longest shortest solution in the cluster) |
| `--threads <int>` | settings `search_threads` | Search threads; `0` selects the automatic value |
| `--help` | – | Print the available options and exit |

With `--space preset`, the board size, goal, restricted/free movement, target block, block inventory and fixed walls are taken from the selected preset. With `--wall-inventory`, the preset's wall shapes replace the fixed walls and the search enumerates wall layouts limited by `--min-walls`/`--max-walls`. The built-in `rh` (Rush Hour, 6×6) and `klotski` (Klotski, 4×5) rulesets remain available as shortcuts. The CLI and the GUI share the same puzzle-space runner and the same `MSPACE` file format, so a CLI result can be opened with **Load Puzzle Space** in the GUI.

`--output`, `--blocks`, `--min-blocks`, `--wall-inventory`, `--min-walls`, `--max-walls` and `--moves` only take effect together with `--space`; `--threads` applies to both headless modes. Wall options require `--space preset`; the built-in `rh` and `klotski` rulesets have no wall shapes. Command line parsing is compiled out on Windows, where the program always starts the GUI.

Examples:

```sh
# GUI with a custom preset file
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- presets/my.puzzle

# GUI with the RushHour + Walls 1 preset (index 4) and its graph already generated
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --preset 4 --populate

# Headless puzzle space from a preset (keeps the preset's walls)
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --space preset --preset 4 --blocks 2 --moves 0 --output rh_walls.puzzles

# Search wall layouts from the preset's wall shapes (0..2 wall pieces)
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --space preset --preset 4 --wall-inventory --min-walls 0 --max-walls 2 --blocks 2 --moves 0 --output rh_wall_inventory.puzzles

# Headless Rush Hour ruleset
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --space rh --blocks 4 --moves 10 --output rh.puzzles

# Headless Klotski ruleset
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --space klotski --blocks 4 --moves 10 --output klotski.puzzles

# 4x4 Lights Out with its graph populated
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --puzzle lights_out --populate

# 10-disk Towers of Hanoi (preset 27) with its full 59,049-state graph populated
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --preset 27 --populate

# 8-puzzle hard start with its full 181,440-state graph populated
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --puzzle eight_puzzle --populate

# 4x4 Sudoku with its full 10,898-state partial-state graph populated
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --puzzle sudoku4 --populate

# Headless state-space benchmark for the 10-disk Hanoi preset
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --state-space --preset 27

# Headless state-space benchmark for the first 8-puzzle preset
nix run git+https://gitea.local.chriphost.de/christoph/cpp-masssprings -- --state-space --puzzle eight_puzzle
```

`--state-space` loads the selected preset, explores its full reachable component without opening a window or starting the physics/graphics pipeline, and prints the puzzle type, preset, state and link counts, completion/cancellation status, elapsed time, states per second and tracked memory. It is intended for benchmarking.

### Puzzle types and runtime switching

The GUI can switch puzzle types at runtime with **F1** or the **Puzzle** spinner in the top bar. The currently available types are:

- `sliding_block` — the Rush Hour / Klotski-style board.
- `lights_out` — 4×4 Lights Out. Click a pressable cell to toggle it (binary) or advance it (ternary) together with its neighbours. **F2** randomizes the starting state and **F3** resets it to the target. The preset file includes classic, diagonal, wrap-around, restricted ternary and a 20-move hardest variant.
- `hanoi` — Towers of Hanoi on three pegs, with 3-, 5- and 10-disk presets. Click a peg to pick up its top disk, then click another peg to drop it. **F2** randomizes the disk positions and **F3** resets to the solved tower. The 10-disk graph has 3¹⁰ = 59,049 states and a 1,023-move optimal solution from the start.
- `eight_puzzle` — the classic 3×3 sliding-tile puzzle with eight numbered tiles and one blank. Click a tile next to the blank, or use **WASD**/arrow keys to move the blank. **F2** randomizes the board and **F3** resets to the solved arrangement. The full reachable component has 181,440 states; the included hard start needs 31 moves.
- `sudoku4` — 4×4 Sudoku with 2×2 boxes. Select a digit with **1**–**4** or the digit buttons, then click an empty cell to place it. Right-click a non-given cell to clear it. **F3** resets to the givens. The included puzzle has a unique solution and a 10,898-state partial-state graph.

The window is split into a narrow left column and a wide graph column. The left column holds the always-visible, scrollable **Controls** pane in its upper half and the puzzle board in its lower half. The controls show only what is currently applicable to the active puzzle. **`,`** (or the **Settings** button) replaces the Controls pane with the Settings pane; the puzzle board and graph stay visible and interactive. Press **?** at any time to open the help overlay with **Hotkeys**, **State Space** and **Puzzle Space** tabs. For sliding-block puzzles, **Tab** toggles between Puzzle Mode and Edit Mode; while editing, every board piece is drawn with diagonal hatching so the active mode is visible without looking at the controls. Pressing **Y** converts the selected block into a wall with the block's full footprint; pressing **Y** on a wall converts the whole wall piece back into a movable block. With no block selected, **Y** places a 1×1 wall at the hovered cell. Starting a block-add preview and then pressing **Y** creates a wall of the previewed rectangle directly.

The **Graph** section contains a **3D Graph / 2D Graph** toggle. 3D is the default force-directed layout. In 2D mode the layout is constrained to the XY plane, existing 3D depth is preserved while the mode is active, and the camera becomes a fixed top-down orthographic view. Rotation is disabled in 2D mode; panning and wheel zooming remain available.

Preset files can mix types using an `@type <id>` line before a preset; files without a type tag default to `sliding_block` for backwards compatibility.

### Puzzle-space search in the GUI

For sliding-block puzzles the Controls pane contains a **Search** mode toggle with **State Space** and **Puzzle Space** modes. The puzzle-space mode is puzzle-specific: in the **Puzzle Space** section, the board size, goal, restricted/free movement, target block and block inventory are taken from the current board. The remaining search limits are editable:

- **Min Blocks** defaults to 2 and drops smaller block multisets. The target counts towards both limits, and a valid puzzle always needs the target plus at least one movable block, so 1 behaves like 2.
- **Max Blocks** defaults to the current number of target + movable blocks. Walls do not count towards this limit.
- **Use wall shapes as inventory** is off by default. When enabled, the shapes of the walls on the current board become a wall inventory and the search varies their positions. Fixed walls are replaced by this mode. **Min Walls** defaults to 0 and **Max Walls** defaults to the number of derived wall pieces. Wall shapes are reconstructed greedily from the wall mask, so adjacent wall cells may merge into one larger shape. There is no hard layout cap; large wall inventories can be extremely expensive, so choose Max Walls carefully.
- **Min Moves** defaults to 10 and filters out clusters whose hardest solution is shorter.

The search runs in the background on a dedicated worker pool, so the graph keeps rendering and the search can be cancelled. A single phase-weighted progress bar covers preparation, exact placement counting, memory projection and partition exploration, so it never restarts between phases. The text below it explains the current phase and reports weighted progress and cluster counts (with thousands separators), the elapsed time and a smoothed ETA; the tracked RAM breakdown remains in the memory overlay. The exact placement count is obtained by a fast counting pass before the expensive exploration; it is also the exact search-space size, because every placement is enumerated once. Progress is published while a partition's enumeration, component discovery and distance BFS run, not only when the partition finishes.

The search is split into independent partitions, one per exact block multiset (the target plus a fixed set of movable blocks) and, when wall inventory is enabled, per fixed wall layout. A move never changes the multiset or the wall layout, so components never cross partitions and each partition can be explored, and streamed, on its own. Partitions are processed in batches with a fixed 64M-placement budget, so more memory cannot increase the batch size. Batch partitions use a two-bit component/distance sweep: one bitset marks component membership, one marks distance levels, and unwinnable components are never traversed; no per-state union-find, root-index or distance array is needed. A partition that does not fit a batch, or a batch that cannot fill the worker pool, is processed with intra-partition parallelism (lock-free union-find and a level-synchronous distance BFS). The search panel and the RAM overlay show live figures: the search memory follows the states currently in flight (one partition per worker), and the projected peak follows the largest observed in-flight set. There is currently no configurable RAM limit and no confirmation prompt; the projected peak is informational only and never gates the search. All memory figures are approximate sums of the program's own containers, not the process RSS. The reported application memory includes the search containers, the active puzzle model and the state graph.

When the search finishes, all discovered clusters are kept in memory sorted by hardness. The default order is most moves first; an alternative orders by most blocks, then fewest states, then most moves. The hardest result under the selected order is loaded into the board automatically. The result selector is a slider with **Prev** and **Next** buttons, and moving it updates the board immediately without a separate load button. Switching the sort mode resets the selection to the first result. **Save Puzzle Space** and **Load Puzzle Space** open filename prompts for writing/reading the explored results (default `puzzle_space.puzzles`). To explore a loaded result's state space, switch to State Space mode and press **Populate Graph**. A loaded result is an in-memory custom state, not a preset; the normal **Save as Preset** action can persist it. By default, walls on the current board are treated as fixed board geometry: the generator only enumerates target and inventory placements that fit on the free cells, and every returned cluster keeps the wall mask. With wall inventory enabled, the wall shapes are searched instead; each returned cluster keeps its own wall layout, wall piece count and blocks.

Both state-space and puzzle-space searches are asynchronous jobs. The state-space job explores a clone of the active model and installs the result on the main thread when it finishes, so populating large graphs no longer freezes the UI. At log level 1 searches log start/finish summaries; at level 2 they also log phase timings and per-second placement/state/cluster counts.

### Settings

The **Settings** button in the top bar (or the **`,`** key) opens a live settings overlay: Barnes-Hut parameters, tree depth and rebuild interval, integration/physics constants, link and constraint parameters, camera speeds, rendering options (node size, solution/visited node sizes, edges, edge LOD stride, billboard nodes, view labels, physics debug, performance graph and RAM overlay visibility), physics and search worker counts, task block sizes and the log level (0 = off, 1 = info, 2 = detailed). Search threads apply to the next search without a restart; the Performance tab shows both the active pool size and the value that will be used next. A rolling 10-second physics/render timing graph is always visible in the top-right of the graph pane, and a tracked-memory breakdown is always visible in the bottom-right of the graph pane. The latter counts our own containers and known GPU buffers, and a global new/delete interposer reports live and peak C++ heap allocation. It excludes malloc/C allocations, OS/driver memory, thread stacks and allocator overhead. Changes apply immediately where possible; the tree depth is picked up by the next rebuild, the async-octree and CPU/GPU switches are applied on the next physics step, and the worker-thread count is applied at startup. GPU mode runs the force, integration and constraint stages through OpenGL 4.3 compute shaders; the octree is still built on the CPU.

Settings are persisted to `settings.ini` in the working directory on a clean exit and loaded at startup. **Reset All** restores the defaults defined in `src/default_settings.cpp`.

## Technical

### Architecture

The application is split into these layers:

- `state_model` — the puzzle-specific state representation, rules, exploration and serialization. `sliding_block_model` wraps the original packed `puzzle`; `lights_out_model` is rules-driven and encodes base-2 or base-3 states in a 64-bit integer; `hanoi_model` packs the peg of every disk into two bits per disk; `eight_puzzle_model` packs nine tile values into four bits each; `sudoku4_model` packs sixteen cell values into four bits each and keeps the givens separately.
- `state_graph` — puzzle-independent graph bookkeeping: unique-state indices, links, move history, winning states, shortest paths and visit counts. It never includes a puzzle header. It supports directed graphs; ternary Lights Out moves are not involutions, so their solution distances are computed with a reverse BFS from the target.
- `state_manager` — owns the active `puzzle_context` (model + presenter + codec) and the `state_graph`, and keeps the physics engine in sync. `puzzle_context` is stable while its contents can be replaced at runtime.
- `cpu_layout_engine`, `cpu_spring_system` and `octree` — the background physics thread that iteratively lays out the graph.
- `gpu_context` — optional hidden OpenGL 4.3 context for the GPU physics backend.
- `puzzle_space` — the shared puzzle-space configuration, search runner, progress counters and result types used by both the CLI and the GUI.
- `placement_space` — exact block-multiset partitions, the canonical placement enumerator, compact placement keys and the state table used by the puzzle-space runner. Fixed walls are applied as an initial occupancy mask and pruned from the target range and area bound; they are not part of the placement key. With wall inventory enabled, each generated wall layout is an additional partition dimension and carries its own wall mask. Keys are packed into the minimum number of bytes (4–12 instead of 8/16), and a CHD minimal perfect hash maps key → dense id without a separate slot array. The goal states are collected while the index is built and remapped to their rank ids, so a batch partition is enumerated only once; its keys stay resident for the sweep, which carries them only through its BFS frontiers. Each shape carries precomputed coverage and per-direction target-cell tables, so a move needs no division or bounds switch, and the moved group of the key is spliced in place instead of repacking all blocks. It is an implementation detail of `puzzle_space` but is kept separate and unit-tested.
- `search_controller` — owns the background exploration jobs, progress snapshots, cancellation and result handoff to the main thread.
- `cli` — argument parsing and the headless puzzle-space and state-space modes, including progress reporting and SIGINT cancellation.
- `renderer`, `user_interface` and `input_handler` — the raylib front end with the graph view, the puzzle-specific board presenter and the menu.

Puzzle-specific drawing, editing and key handling live behind `puzzle_presenter`; the UI only provides the pane, grid helpers and generic graph/camera controls. Input flows from the user through `state_manager` into the layout engine. The renderer only reads the latest positions published by the physics thread.

Search jobs use their own worker pool (`search_threads` in the Performance settings; 0 selects half the hardware threads; changes apply to the next search) so a long exploration does not compete with the physics pool. State-space jobs clone the model before exploring it; the completed model and links are installed only on the main thread. Puzzle-space results are stored independently of the active board, so they remain available after switching presets or puzzle types. Only one search job runs at a time; starting a new one requires the previous job to finish or be cancelled. There is no automatic RAM gate or confirmation prompt; the projected peak is informational only, and the user can cancel the search manually.

### Puzzle representation

A sliding-block puzzle occupies 40 bytes: a 32-byte block/meta representation (4 × `u64`) plus a separate 64-bit wall-cell mask. The block representation starts with a 16-bit metadata field (width, height, goal position, restricted-movement flag and goal-present flag) followed by 15 block slots. Each block is packed into a 16-bit integer holding its position, size minus one, and its target flag. Walls are not stored in the block array; the wall mask has one bit per cell and therefore supports many small walls without consuming block slots. The block array is kept sorted, so equal states have identical memory representations and are compared and hashed as raw integers; the wall mask is part of equality, ordering and hashing. Every state also exposes 64-bit board bitmaps (occupancy, horizontal and vertical coverage), which are used to validate moves and placements without scanning the block list. Boards may be 3×3 to 8×8 and hold at most 15 target/movable blocks plus any number of wall cells.

A Lights Out state is a base-N number encoded in a `u64`. Classic 4×4 is a 16-bit mask; the ternary variants use base-3 digits. The rules object controls the grid size, base, neighbour pattern (orthogonal/diagonal and optional wrap-around), pressable cells and target pattern. Both representations are hidden behind `state_model`.

A Towers of Hanoi state stores the peg of every disk as two bits in a `u32`, with disk 0 as the smallest disk. The default has 10 disks, so the full state space is 3¹⁰ = 59,049 states. The model supports up to 12 disks for full exploration.

An 8-puzzle state packs the nine cell values into four bits each (36 bits in a `u64`). The blank is `0`, and the solved state is `1..8` followed by the blank. Only half of the 9! permutations are reachable from a given start, so a full exploration contains 181,440 states.

A 4×4 Sudoku state packs the sixteen cell values into four bits each in a `u64`, with `0` for an empty cell. The givens are stored separately in the model and cannot be changed by moves. Filling an empty cell and clearing a filled non-given cell are both legal moves, so the state graph is undirected. The included puzzle has four givens and a 10,898-state partial-state graph.

### State-space generation

In `--space` mode a puzzle space is derived either from the selected preset board or from a built-in ruleset:

1. Generate every exact multiset of permitted block shapes that fits on the board, with the target block counting towards the `--blocks` limit and the number of blocks staying between `--min-blocks` and `--blocks`. With `--wall-inventory`, also enumerate wall layouts from the preset's wall shapes between `--min-walls` and `--max-walls`; each layout is an additional partition dimension.
2. For each multiset, enumerate every placement of exactly those blocks once: the next empty cell found by the occupancy bitmap is either left empty, given the target (if still unplaced and inside its reachable range), or given a remaining block. The placement set of a multiset is closed under legal moves, so every state belongs to exactly one partition and no global deduplication is needed.
3. Explore each partition independently: enumerate its states into a compact key table (6 bits per block cell, packed into the minimum byte width) with a CHD minimal perfect hash index. The goal states of a partition are collected during that enumeration (they are the sweep's seeds) and remapped by the index build, so nothing is enumerated twice; a batch sweep carries keys only through its BFS frontiers. Batch partitions run a two-bit component/distance sweep (component membership + BFS level, unwinnable components skipped), and the distance walk is skipped when the component's seed eccentricity already proves that no state can reach the required number of moves. Giant partitions use lock-free union-find and a level-parallel multi-source BFS.
4. Components never cross partitions. A cluster is identified by its numerically smallest state, and its hardest state is the numerically smallest of the states at the component's maximum distance, so both are independent of the order in which states are enumerated or hashed; its state count and hardness are computed from the component.
5. A cluster is only kept if its largest distance (the longest shortest solution) is at least `--moves`; the remaining clusters are written to the output file in the same `MSPACE` format used by the GUI.

Partitions are processed in batches with a fixed 64M-placement budget, and huge partitions (or batches that cannot fill the worker pool) use intra-partition parallelism. Peak memory therefore scales with the largest batch, not with the total number of states, which is what makes much larger puzzle spaces feasible.

### Preset file format

A preset is a single line, preceded by exactly one comment line. An optional `@type <id>` line selects the puzzle type; without it the preset is a `sliding_block` preset.

```
# Klotski
S:[4x5] G:[1,3] M:[F] B:[{1x2 2X2 _ 1x2} {_ _ _ _} {1x2 2x1 _ 1x2} {_ 1x1 1x1 _} {1x1 _ _ 1x1}]

@type lights_out
# Lights Out 4x4
LO:[4x4] B:[1111111111111111]

@type lights_out
# Lights Out 4x4 Hardest (3-state, wrap, restricted)
LO:[4x4] V:[3] N:[orth_wrap] P:[0110111000111110] T:[0000000000000000] B:[0111211000122110]

@type hanoi
# Towers of Hanoi 10
H:[10] T:[2] B:[0000000000]

@type eight_puzzle
# 8-Puzzle Hard (31 moves)
P8:[3x3] B:[8672543_1]

@type sudoku4
# 4x4 Sudoku
S4:[4x4] G:[1_____3__2_____4] B:[1_____3__2_____4]
```

The sliding-block fields are:

- `S:[WxH]` - board size (3 to 8 per axis)
- `G:[x,y]` - goal cell for the target block (optional)
- `M:[R|F]` - restricted movement (blocks only move along their longer axis, square blocks in any direction) or free movement
- `B:[{...} ...]` - the board rows

Inside the braces every cell contains either `_` or a block sized `W`×`H` that is anchored at its top-left corner. Movable blocks use `WxH`, the target block uses `WXH` and immovable walls use `W*H`. Cells covered by a block anchored earlier remain `_`. The preset syntax is unchanged: wall tokens are read into the separate wall mask and written back as `1*1` wall cells, so older presets with `W*H` walls continue to load.

The Lights Out fields are:

- `LO:[WxH]` - grid size
- `V:[2|3]` - number of light states (default 2)
- `N:[orth|diag|orth_wrap|diag_wrap]` - neighbour pattern (default `orth`)
- `P:[bits]` - pressable cells, row-major (default all pressable)
- `T:[digits]` - target pattern (default all zero)
- `B:[digits]` - starting state, row-major

The hardest Lights Out preset has 59,049 reachable states and a 20-move optimal solution. It uses ternary lights, wrap-around neighbours and only 10 pressable cells.

The Towers of Hanoi fields are:

- `H:[n]` - number of disks (1 to 12, default 10)
- `T:[0|1|2]` - target peg (default 2)
- `B:[digits]` - peg of each disk, smallest disk first (`0` to `2`)

The default 10-disk start `H:[10] T:[2] B:[0000000000]` has a 1,023-move optimal solution.

The 8-puzzle fields are:

- `P8:[3x3]` - board size (only 3×3 is supported)
- `B:[digits]` - nine cells, row-major; use `1`–`8` for tiles and `_` (or `0`) for the blank

The included hard start `P8:[3x3] B:[8672543_1]` has a 31-move optimal solution. Unsolvable permutations are rejected by the codec.

The 4×4 Sudoku fields are:

- `S4:[4x4]` - board size (only 4×4 is supported)
- `G:[digits]` - givens, row-major; use `1`–`4` for givens and `_` for empty cells
- `B:[digits]` - current state, row-major; use `1`–`4` for filled cells and `_` for empty cells

At least four givens are required. The included puzzle `G:[1_____3__2_____4]` has the unique solution `1342243142133124`.

Each preset line has to be accompanied by one `#` comment line, which is shown as the preset's name.

### Graph layout and physics

Each puzzle state is a mass and each legal move is a link. Links have a **hard minimum length** (`REST_LENGTH`): after integration, compressed links are projected back to that length (position-based dynamics). Above the minimum, a tension-only spring (`SPRING_K`) with velocity damping pulls linked masses together. In addition, all pairs of masses repel each other.

The minimum-length projection is what keeps dense clusters stable: there is no stiff spring/repulsion force balance at short range, which used to cause persistent jitter. Springs are projected in parallel using an edge coloring in which no two links of the same color share a mass.

Repulsion is calculated with Barnes-Hut using a linear octree built from Morton codes of the mass positions; distant nodes are approximated by their center of mass once the node's size-to-distance ratio exceeds `THETA`. Force queries use the exact position snapshot the tree was built from, so a lagged tree does not produce a mismatched force field. The simulation runs in 3D by default (or constrained to the XY plane in 2D graph mode) at 90 updates per second using Verlet integration with slight damping. All tunable constants live in `include/config.hpp`.

New masses are spawned close to their neighbor, so the graph initially "explodes" and then settles into clusters. `OCTREE_REBUILD_INTERVAL` and `CONSTRAINT_INTERVAL` control how often the tree and the link projection are refreshed; both default to every step for stability.

### Threading model

The layout engine runs on its own thread. The main thread submits commands (add mass, add spring, clear graph) through a mutex-protected queue, and the physics thread hands its positions back through a mutex-protected exchange with ready/consumed condition variables.

When **Async octree build** is enabled in the settings (the default), the octree for the next frame is built on a dedicated thread while forces are computed from the previous tree; it can be switched at runtime. The optional `BS::thread_pool` (enabled by `THREADPOOL`) parallelizes spring forces, repulsion forces, the link projection and integration. Its worker count can be set in the settings (0 = automatic) and is applied at startup.
