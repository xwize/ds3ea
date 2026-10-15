# Scripts

Python 3 scripts that use only the standard library. Run them from any worktree.

## launch_game.py

Runs your build of the add-on in the real game with no one at the keyboard:

```
python scripts/launch_game.py [--addon PATH] [--game-dir DIR] [--seconds N] [--grace N]
```

1. Refuses to start if `DarkSoulsIII.exe` is already running. A running game locks the add-on file, and only one
   instance runs at a time.
2. Copies `ds3ea.addon64` into the game folder, overwriting the one already there. The source is `--addon`, or by
   default the newest `ds3ea.addon64` under this repo's `build/` folder. No other file in the game folder is
   changed.
3. Starts `DarkSoulsIII.exe` from the game folder (`--game-dir`, default `E:\Games\Dark Souls 3\Game`).
4. After `--seconds` (default 30), sends Alt+F4 to the game window. If the game is still running `--grace` seconds
   later (default 15), the script kills it.
5. Prints this run's part of `ReShade.log`, from ReShade's session header on. That includes everything logged
   through `ds3ea::log`.

The exit code is 0 on success. It is 1 when the add-on couldn't be installed, the game couldn't start, the game
exited before the timer ran out, or ReShade wrote no log during the run.

Build first, for example `cmake --build --preset vs-release`. The script doesn't build anything itself.
