"""Install our add-on into Dark Souls 3, run the game for a while, close it and print this run's ReShade log.

By default the script presses E a few times once the game window appears, to get past the title screen into the game.

Usage: python scripts/launch_game.py [--addon PATH] [--game-dir DIR] [--seconds N] [--grace N] [--title-only]
See scripts/README.md.
"""

import argparse
import ctypes
import ctypes.wintypes as wintypes
import shutil
import subprocess
import sys
import time
from pathlib import Path

GAME_EXE = "DarkSoulsIII.exe"
ADDON_NAME = "ds3ea.addon64"
DEFAULT_GAME_DIR = Path(r"E:\Games\Dark Souls 3\Game")
REPO_ROOT = Path(__file__).resolve().parent.parent
SESSION_HEADER = "Initializing crosire's ReShade"

user32 = ctypes.WinDLL("user32", use_last_error=True)

VK_MENU = 0x12
VK_F4 = 0x73
SCAN_E = 0x12
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_SCANCODE = 0x0008
SW_RESTORE = 9

# The intro plays for about 25s after the window appears and ignores E, so the presses run until the save is loading
TITLE_SCREEN_PRESSES = 30
PRESS_INTERVAL_SECONDS = 1.0
PRESS_HOLD_SECONDS = 0.08


def fail(message: str) -> int:
	print(f"error: {message}", file=sys.stderr)
	return 1


def game_is_running() -> bool:
	result = subprocess.run(
		["tasklist", "/FI", f"IMAGENAME eq {GAME_EXE}", "/NH", "/FO", "CSV"],
		capture_output=True, text=True, check=True)
	return f'"{GAME_EXE}"'.lower() in result.stdout.lower()


def find_newest_addon() -> Path | None:
	candidates = list((REPO_ROOT / "build").glob(f"**/{ADDON_NAME}"))
	return max(candidates, key=lambda path: path.stat().st_mtime, default=None)


def find_main_window(pid: int) -> int | None:
	"""Returns the first visible top-level window owned by the process, if it has one yet."""
	found = []

	@ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
	def on_window(hwnd, _):
		owner = wintypes.DWORD()
		user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
		if owner.value == pid and user32.IsWindowVisible(hwnd) and not user32.GetWindow(hwnd, 4):  # 4 = GW_OWNER
			found.append(hwnd)
			return False
		return True

	user32.EnumWindows(on_window, 0)
	return found[0] if found else None


def press_e(hwnd: int) -> None:
	if user32.GetForegroundWindow() != hwnd:
		user32.ShowWindow(hwnd, SW_RESTORE)
		# Windows only lets the process that received the last input take the foreground, so a key event goes first
		user32.keybd_event(VK_MENU, 0, 0, 0)
		user32.SetForegroundWindow(hwnd)
		user32.keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0)
	# The game reads the keyboard by scan code and ignores virtual-key-only events
	user32.keybd_event(0, SCAN_E, KEYEVENTF_SCANCODE, 0)
	time.sleep(PRESS_HOLD_SECONDS)
	user32.keybd_event(0, SCAN_E, KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0)


def send_alt_f4(hwnd: int) -> None:
	user32.ShowWindow(hwnd, SW_RESTORE)
	# Windows only lets the process that received the last input take the foreground, so a key event goes first
	user32.keybd_event(VK_MENU, 0, 0, 0)
	user32.SetForegroundWindow(hwnd)
	time.sleep(0.2)
	user32.keybd_event(VK_F4, 0, 0, 0)
	user32.keybd_event(VK_F4, 0, KEYEVENTF_KEYUP, 0)
	user32.keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0)


def close_game(process: subprocess.Popen, grace_seconds: float) -> None:
	hwnd = find_main_window(process.pid)
	if hwnd:
		print("Sending Alt+F4 to the game window")
		send_alt_f4(hwnd)
	else:
		print("The game has no window to send Alt+F4 to")
	try:
		process.wait(timeout=grace_seconds)
	except subprocess.TimeoutExpired:
		print(f"The game is still running after {grace_seconds:g}s; terminating it")
		process.kill()
		process.wait()


def print_run_log(log_path: Path, started_at: float) -> bool:
	"""Prints the log lines from this run's session header on. Returns False if this run wrote no log."""
	print(f"\nReShade log: {log_path}")
	if not log_path.exists() or log_path.stat().st_mtime < started_at:
		print("ReShade didn't write a log during this run (is ReShade installed in the game folder?)")
		return False
	lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
	headers = [index for index, line in enumerate(lines) if SESSION_HEADER in line]
	start = headers[-1] if headers else 0
	print("-" * 80)
	print("\n".join(lines[start:]))
	print("-" * 80)
	return True


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
	parser.add_argument("--addon", type=Path,
		help=f"add-on to install (default: the newest {ADDON_NAME} under {REPO_ROOT / 'build'})")
	parser.add_argument("--game-dir", type=Path, default=DEFAULT_GAME_DIR,
		help=f"folder holding {GAME_EXE} (default: {DEFAULT_GAME_DIR})")
	parser.add_argument("--seconds", type=float, default=60,
		help="how long to run the game (default: 60, as loading into the game takes about 40s)")
	parser.add_argument("--grace", type=float, default=15,
		help="how long to wait for the game to exit after Alt+F4 before terminating it (default: 15)")
	parser.add_argument("--title-only", action="store_true",
		help=f"stay on the title screen instead of pressing E {TITLE_SCREEN_PRESSES} times to get into the game")
	args = parser.parse_args()

	game_exe = args.game_dir / GAME_EXE
	if not game_exe.is_file():
		return fail(f"{game_exe} not found; pass --game-dir")
	# A running game locks the add-on, so check before copying
	if game_is_running():
		return fail(f"{GAME_EXE} is already running; close it first")

	addon = args.addon or find_newest_addon()
	if addon is None:
		return fail(f"no {ADDON_NAME} found under {REPO_ROOT / 'build'}; build the add-on or pass --addon")
	if not addon.is_file():
		return fail(f"{addon} not found")
	try:
		shutil.copyfile(addon, args.game_dir / ADDON_NAME)
	except OSError as error:
		return fail(f"can't install {addon}: {error}")
	print(f"Installed {addon.resolve()} -> {args.game_dir / ADDON_NAME}")

	# ReShade.log's modification time tells this run's log apart from an old one; allow for coarse timestamps
	started_at = time.time() - 2
	try:
		process = subprocess.Popen([str(game_exe)], cwd=args.game_dir)
	except OSError as error:
		return fail(f"can't start {game_exe}: {error}")
	print(f"Started {GAME_EXE} (pid {process.pid}); closing it in {args.seconds:g}s")

	exited_early = False
	presses_left = 0 if args.title_only else TITLE_SCREEN_PRESSES
	next_press_at = 0.0
	deadline = time.monotonic() + args.seconds
	while time.monotonic() < deadline:
		if process.poll() is not None:
			exited_early = True
			break
		if presses_left and time.monotonic() >= next_press_at:
			hwnd = find_main_window(process.pid)
			if hwnd:
				if presses_left == TITLE_SCREEN_PRESSES:
					print(f"Pressing E {TITLE_SCREEN_PRESSES} times to get past the title screen")
				press_e(hwnd)
				presses_left -= 1
				next_press_at = time.monotonic() + PRESS_INTERVAL_SECONDS
		time.sleep(0.1)

	if presses_left == TITLE_SCREEN_PRESSES and not exited_early:
		print("The game window never appeared, so E wasn't pressed")
	if exited_early:
		print(f"The game exited early with code {process.returncode:#x}", file=sys.stderr)
	else:
		close_game(process, args.grace)
		print(f"The game exited with code {process.returncode:#x}")

	log_written = print_run_log(args.game_dir / "ReShade.log", started_at)
	return 1 if exited_early or not log_written else 0


if __name__ == "__main__":
	sys.exit(main())
