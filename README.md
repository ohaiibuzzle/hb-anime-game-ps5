# HoYoverse games on PS5 (prospero-win + Spritz patches)

Downloads and runs the games Spritz-Wine supports, Genshin Impact, Honkai:
Star Rail and Zenless Zone Zero, through
[prospero-win](https://github.com/mpereiraesaa/prospero-win). Each game is
two entries in prospero-win's launcher that share one Wine prefix:

| Entry | Runs | Status |
|---|---|---|
| **&lt;Game&gt; - Download / Update** | `sophon-dl.exe`: installs, resumes, updates and repairs the game over HoYoPlay's Sophon protocol, on the console | Core tested against live data for all three games; full download, kill and resume tested under desktop Wine (Genshin ko-kr voice pack); **not yet run on a PS5** |
| **&lt;Game&gt;** | The game through DXVK | **Untested**; see "Will the games start?" |

| Game | Slug (prefix, recipe, profiles) | `biz` | Installs to | Size with English voice |
|---|---|---|---|---|
| Genshin Impact | `genshin-impact` | `hk4e_global` | `C:\Games\GenshinImpact` | 150 GB (7.1.0) |
| Honkai: Star Rail | `honkai-star-rail` | `hkrpg_global` | `C:\Games\StarRail` | 116 GB (4.6.0) |
| Zenless Zone Zero | `zenless-zone-zero` | `nap_global` | `C:\Games\ZenlessZoneZero` | 67 GB (3.2.0) |

## Why not HoYoPlay itself

`HoyoPlay.exe` installs HoYoPlay 1.18, a Chromium (CEF) app. Chromium needs
child processes, and HoYoPlay starts games as child processes too. Wine on
the PS5 runs a single process (prospero-win patch 0550), so the launcher could
never start a game even if its UI showed up. `sophon-dl` does the one job we
need from HoYoPlay, downloading, using the protocol notes from
[HoyoFetch/SophonKit](https://github.com/ohaiibuzzle/HoyoFetch/tree/senpai/Packages/SophonKit).

## Layout

| Path | What |
|---|---|
| `sophon-dl/` | The downloader: one Win64 GUI program in C (WinHTTP, zstd, cJSON) |
| `wine-patches/` | Spritz-Wine patches renumbered for prospero-win's series |
| `recipes/<slug>.yml` | `pw_install.py` recipes that build each game's prefix |
| `profiles/<slug>.profile` | The Download / Update entries |
| `profiles/<slug>-game.profile` | The game entries |
| `push-game-profile.sh` | Adds a game entry on the console |
| `.github/workflows/build.yml` | CI: the patched app zip and the downloader bundle |

## Spritz-Wine patches

From [spritz-wine 0001-spritz](https://github.com/NelloKudo/spritz-wine/tree/master/patches/0001-spritz).
Each one is checked with `git apply` against prospero-win's pinned Wine
(`490f6d5`), on top of prospero-win's own series:

| Spritz | Here | Why |
|---|---|---|
| 0009 curl timeout fix (Genshin, ZZZ) | `0920` | Ported. The switch is renamed `PW_ENABLE_TIMEOUT_FIX`, because profiles may only set `PW_`/`DXVK_`/… variables. The Genshin and ZZZ game entries set it |
| 0010 crypt32 attribute order | `0921` | Ported as is |
| 0002 block the first connection (Star Rail) | `0922` | Ported. On by itself for `StarRail.exe`, and forced on for Genshin and ZZZ by `PW_ENABLE_DISCONNECT=1` in their game entries (Spritz does not do that for them); the switches are renamed `PW_ENABLE_DISCONNECT`/`PW_DISABLE_DISCONNECT`. Its second hunk is moved onto upstream Wine's `connect()`, which lacks Proton's lines it was anchored to |
| 0003 steam.exe stub from Proton | `0924` | Ported, with the two lines it needs in the committed `configure`. Each prefix gets `C:\windows\system32\steam.exe` from it, but nothing runs it: starting the game through it would take a second process |
| 0004–0005 launch games through the stub (Genshin, ZZZ, Honkai Impact 3rd) | — | The PS5 cannot start a second process; `0923` stands in |
| NelloKudo's kernelbase steam.exe parent hack (not in Spritz's series) | `0923` | Ported, and extended from Genshin (`GenshinImpact.exe`, `YuanShen.exe`) to `StarRail.exe` and `ZenlessZoneZero.exe`. The first time the game asks for another process's path, it answers `c:\windows\system32\steam.exe`, so no stub process is needed. `0924` puts the real stub at that path |
| 0006, 0008 winex11 | — | The PS5 has its own display driver, not X11 |
| 0007 copy arial.ttf | — | Upstream Wine has no `arial.ttf`. The recipes add Liberation Sans as `Arial` instead, which is what that file is |
| 0001 winediag text | — | Cosmetic |

## Setup

**[RUNNING.md](RUNNING.md)** walks through it: GitHub Actions builds the
patched app and all three games' prefixes, and you push them to the console.
You don't need a Linux PC or your own Wine build.

The CI can rebuild so little because the patches only change three PE modules
(`ws2_32.dll`, `crypt32.dll`, `kernelbase.dll`). `.github/workflows/build.yml` takes upstream's
release zip, rebuilds those two from the prospero-win commit the release
names in `SOURCES.txt` with our patches added to the series, and swaps them
in. It then runs upstream's `pw_install.py` on `recipes/`. To do it by hand
instead, follow upstream's
[installing games](https://github.com/mpereiraesaa/prospero-win/blob/main/docs/INSTALLING_GAMES.md)
guide with `wine-patches/*.patch` added to its `wine/patches/`, and replace
each generated `<slug>.profile` with this repo's before pushing (the
downloader needs `graphics = gdi`).

The recipes put Liberation Sans in the prefix and map `Arial` to it, as
Spritz-Wine's `arial.ttf` does (Proton builds that from Liberation Sans).
It is OFL-licensed, so the prefixes can be shared, unlike winetricks'
corefonts.

Other voice packs: edit `arguments` in `profiles/<slug>.profile`
(`game en-us ja-jp …`) before pushing.

ZZZ also offers 109 numbered optional content packs (19.5 GB in 3.2.0).
[Collapse Launcher](https://github.com/CollapseLauncher/Collapse) downloads
them only if the player agrees; here they are left out. Whether the game
fetches the ones it needs by itself is unverified.

## Will the games start?

Nobody knows yet. Watch for these:

- **The steam.exe stub (Genshin, ZZZ).** Spritz starts both through
  Proton's stub as a child process. Here they start directly, and `0923`
  reports `steam.exe` as the parent instead, for Star Rail too. Only Genshin
  is known to check; its use for ZZZ and Star Rail is a guess. Proton's
  stub (`0924`) is at that path, so a game that inspects the file finds the
  real thing. If a game exits early, start here.
- **Anti-cheat.** All three run under desktop Wine. Whether they accept
  prospero-win's environment (in-process wineserver, PS5 memory layout) is
  unknown. Nothing here tries to get around them. Their kernel driver is not
  loaded when the game sees Wine (HoYoverse relaxed this for Steam Deck
  players), so the missing `winedevice.exe` process should not matter;
  prospero-win's ntdll is still Wine's, so the game should see Wine here too.
- **Untested territory.** Every game prospero-win's profiles cover is from
  2008 or earlier (GTA IV is the newest). Its patch notes show work on
  Battle.net and Steam's protection layer, but no modern online game with
  anti-cheat.
- **Memory and GPU.** Recent Unity games on DXVK. GTA IV is the
  heaviest DXVK title prospero-win has tested so far.

Send the Wine log from a failed start (prospero-win's
[debugging guide](https://github.com/mpereiraesaa/prospero-win/blob/main/docs/DEBUGGING_GUIDE.md))
and the next step can be planned from it.
