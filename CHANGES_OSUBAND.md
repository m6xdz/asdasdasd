# OSU!BAND port notes

## 1.1.0 performance and profile pass

- Replaced the playing-state zero-duration reader loop with a small cadence and lowered the reader thread from time-critical to above-normal priority.
- Stopped submitting empty overlay frames while hidden, paced visible frames with the compositor, and throttled repeated window-geometry/display-affinity calls.
- Added cloud profile styles: Legit, Rage, Relax Legit, Relax Rage, Relax + Aim, Aim + Assist, Autobot, Tap and Replay. Profiles are selected on the website and applied between maps; no `.cfg` file is downloaded.
- Renamed the user-facing Lab channel to Beta; the internal `lab` value remains unchanged for server/release compatibility.
- Beta loader shows Session and Settings only. The site address and connection internals are fixed for regular users.

## Stability and diagnostics

- Relax uses a bounded look-ahead queue, deterministic release ownership, and releases held keys on map/foreground changes.
- Background workers are guarded; a stopped worker is reported and a small `last-crash.txt` is left under `%LOCALAPPDATA%\\OSUBAND\\Beta`.
- The UI copies only lightweight status data, not the full beatmap, for its frame.

## Runtime

The known-working osu!lazer reader from the 2026.804.2 clean runtime port is retained. Gameplay module source files were not edited.

## Lazer-only cleanup

Removed:
- `core/game/osu_stable.hxx`
- `impl/defs/offsets_stable.hxx`
- stable attach branch from `client_factory.hxx`
- stable parser ownership from the cache
- stable-only songs path override UI

The generic `.osu` text parser used by lazer's file-store loader was renamed from `stable_parser.hxx` to `osu_file_parser.hxx`; its parsing logic was preserved.

## Interface

The new menu is inspired by the information architecture of modern sidebar cheat UIs, but uses an original OSU!BAND visual language:
- midnight/navy glass panels
- cyan + osu-inspired pink dual accents
- animated top accent rail
- animated waveform details
- grouped GAMEPLAY / UTILITY navigation
- animated vector OSU!BAND pulse logo
- live LAZER READY state pill
- safe profile slot in the lower-left

Existing control values and module wiring remain unchanged.

## Loader

The optional loader:
- detects `osu!.exe`
- can start the default `%LOCALAPPDATA%\\osulazer\\current\\osu!.exe`
- starts `OSUBAND.exe` from the loader directory
- uses Discord account avatar and remotely controlled Beta appearance settings
- keeps cloud configs on the website and client; the loader does not expose a config library
- performs no DLL injection
