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
| 0003–0005 steam.exe stub (Genshin, ZZZ, Honkai Impact 3rd) | — | The stub starts the game as a second process, which the PS5 cannot do |
| 0006, 0008 winex11 | — | The PS5 has its own display driver, not X11 |
| 0007 copy arial.ttf | — | Upstream Wine has no `arial.ttf`. The recipes install corefonts instead |
| 0001 winediag text | — | Cosmetic |

## Setup

You need the prospero-win setup from its
[installing games](https://github.com/mpereiraesaa/prospero-win/blob/main/docs/INSTALLING_GAMES.md)
guide: an x86_64 Linux PC with the pinned host Wine, and the console with
FTP and the app.

1. **Get the patched app from CI.** The patches only change two PE modules
   (`ws2_32.dll`, `crypt32.dll`), so `.github/workflows/build.yml` takes
   upstream's latest release zip, rebuilds those two from the prospero-win
   commit the release names in `SOURCES.txt` with our patches added to the
   series, and swaps them in (about an hour on GitHub's runners). Run
   **Build** from the Actions tab (it can take an upstream tag), or push a
   `v*` tag to publish the result as a release. Install
   `prospero-win-<tag>-spritz.zip` like an upstream release. Doing it by
   hand: copy `wine-patches/*.patch` into prospero-win's `wine/patches/`
   and follow its development guide.
2. **Build the downloader:** `make -C sophon-dl` (needs `x86_64-w64-mingw32-gcc`).
   `make -C sophon-dl check` runs the self-test against live HoYoverse data
   for all three games (macOS, because it uses CommonCrypto for MD5).
3. **Build a game's prefix** on the PC, with `<slug>` from the table above:
   ```sh
   python3 tools/pw_install.py <this repo>/recipes/<slug>.yml \
       --library ~/prospero-library --wine <host wine> \
       --file sophon=<this repo>/sophon-dl/sophon-dl.exe
   cp <this repo>/profiles/<slug>.profile ~/prospero-library/profiles/
   python3 tools/pw_prefix.py push <slug> --library ~/prospero-library --host <PS5 IP>
   ```
   The `cp` replaces the generated profile with the GDI one the downloader
   needs.
4. **Download** on the console: start **&lt;Game&gt; - Download / Update**.
   Check the free space against the table above. Leave it running. If it
   stops, start it again and it resumes from the chunks already on disk.
5. **Add the game entry:** `./push-game-profile.sh <slug> <PS5 IP>`, then
   start the game.

Updates: run the Download / Update entry again. If the installed version
is older, it hashes every file and fetches only the chunks that changed.
Other voice packs: edit `arguments` in `<slug>.profile`
(`game en-us ja-jp …`).

ZZZ also offers 109 numbered optional content packs (19.5 GB in 3.2.0).
[Collapse Launcher](https://github.com/CollapseLauncher/Collapse) downloads
them only if the player agrees; here they are left out. Whether the game
fetches the ones it needs by itself is unverified.

## Will the games start?

Nobody knows yet. Watch for these:

- **The steam.exe stub (Genshin, ZZZ).** Spritz starts both through
  Proton's stub as a child process. Here they start directly. If one exits
  early, that's the first thing to look into, and any fix has to work inside
  a single process. Star Rail is not on Spritz's stub list.
- **Anti-cheat.** All three run under desktop Wine. Whether they accept
  prospero-win's environment (in-process wineserver, PS5 memory layout) is
  unknown. Nothing here tries to get around them.
- **Memory and GPU.** Recent Unity games on DXVK. GTA IV is the
  heaviest DXVK title prospero-win has tested so far.

Send the Wine log from a failed start (prospero-win's
[debugging guide](https://github.com/mpereiraesaa/prospero-win/blob/main/docs/DEBUGGING_GUIDE.md))
and the next step can be planned from it.
