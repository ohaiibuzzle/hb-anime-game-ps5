# Running it: from GitHub Actions to playing on the PS5

The **Build** workflow (`.github/workflows/build.yml`) makes everything you
need, so you don't build Wine or make prefixes on your own PC. This page
covers running it, what it produces, and putting the results on the console.
[README.md](README.md) explains why it's built this way.

## 1. Put the repo on GitHub

```sh
git init && git add . && git commit -m "Spritz-PS5"
gh repo create spritz-ps5 --private --source . --push
```

`.gitignore` keeps out `HoyoPlay.exe` (HoYoverse's installer, not ours to
redistribute), `sophon-dl.exe` and the self-test's downloads. Actions are on by
default; if the repo's Actions tab says they are off, turn them on there.

## 2. Run the workflow

It runs by itself on every push to `main` and on pull requests. To run it by
hand, or against a specific upstream release:

- **Actions tab → Build → Run workflow**. "Upstream prospero-win release
  tag" defaults to `latest`; type a tag such as `v0.2.0` to pin one.
- Or from a terminal: `gh workflow run build.yml -f prospero_release=v0.2.0`

A run takes about an hour, almost all of it compiling Wine.

| Job | What it does |
|---|---|
| **sophon-dl and launcher files** | Builds `sophon-dl.exe` and zips it with the profiles, recipes and `push-game-profile.sh` |
| **prospero-win app with Spritz patches** | Downloads upstream's release and checks it against their `SHA256SUMS`. Builds Wine from the prospero-win commit that release names in `SOURCES.txt`, with `wine-patches/` added, and puts the patched `ws2_32.dll`, `crypt32.dll` and `kernelbase.dll` into the app. Then builds the three games' prefixes with upstream's `pw_install.py` |
| **release** | Only for a `v*` tag: publishes the files below as a GitHub release |

## 3. Get the results

On the run's page, under **Artifacts**, or with `gh run download <run id>`:

| Artifact | Holds |
|---|---|
| `prospero-win-spritz` | `prospero-win-<tag>-spritz.zip`: the PS5 app (`PPSA99995`) |
| | `prospero-library-spritz.tar.gz`: `prefixes/` and `profiles/` for all three games |
| | `SHA256SUMS` for both |
| `spritz-ps5` | `spritz-ps5.zip`: `sophon-dl.exe`, `profiles/`, `recipes/`, `push-game-profile.sh` |

Artifacts expire after 90 days by default. In a private repo only people
with access can download them; in a public one, anyone signed in to GitHub
can. For a lasting copy, push a tag: `git tag v1 && git push origin
v1`. The **release** job then attaches the same files to a release.

Check the downloads first: `sha256sum -c SHA256SUMS`.

## 4. Install the app on the PS5

Exactly as upstream's
[getting started](https://github.com/mpereiraesaa/prospero-win/blob/main/docs/GETTING_STARTED.md#1-install-the-app):
unzip `prospero-win-<tag>-spritz.zip` and upload the `PPSA99995` folder to
`/data/homebrew/` over FTP, replacing any older one. Your games and saves in
`/data/prospero-win` are kept. If the tile fails to start, give `eboot.bin`
and the `.prx` files execute permission, as that guide describes.

## 5. Put a game on the console

You need Python 3 and a checkout of prospero-win, for `pw_prefix.py`:

```sh
git clone https://github.com/mpereiraesaa/prospero-win
mkdir -p ~/prospero-library
tar -xzf prospero-library-spritz.tar.gz -C ~/prospero-library
```

Use `tar`: the prefixes contain symbolic links that most unzip tools lose.

Then for each game you want (`genshin-impact`, `honkai-star-rail` or
`zenless-zone-zero`):

```sh
python3 prospero-win/tools/pw_prefix.py push genshin-impact \
    --library ~/prospero-library --host <PS5 IP> \
    --cpu-dll PPSA99995/win/wine/lib/wine/x86_64-windows/wowprospero.dll
```

Don't copy the prefix over by hand: the push also converts its symbolic
links and WoW64 setting for the console, adds the game to the launcher, and
records what it sent so later pushes and pulls keep your saves safe.
`--cpu-dll` is only needed the first time; the path above is inside the
`PPSA99995` folder you unzipped in step 4, so adjust it to where that is.

The launcher now shows **&lt;Game&gt; - Download / Update**.

## 6. Download and play

1. Start **&lt;Game&gt; - Download / Update** and leave it running. Genshin
   needs about 150 GB free, Star Rail 116 GB and ZZZ 67 GB. If it stops,
   start it again: it resumes. It closes itself 10 seconds after "Done"
   (Cross closes it sooner, or after an error).
2. Add the game's own entry from your PC (it's in `spritz-ps5.zip`):
   ```sh
   ./push-game-profile.sh genshin-impact <PS5 IP>
   ```
3. Start the game. Whether it gets past the title screen is still unknown;
   see "Will the games start?" in the README.

Game updates: run the Download / Update entry again.

## When upstream releases a new version

Run the workflow again (it picks up `latest`), then replace `PPSA99995` on
the console as in step 4. Your prefixes stay as they are: don't push the new
library over a game you already set up, or it will refuse (it protects saves
the console changed) unless you pull first. The new library is only for
games you haven't set up yet.

## When the workflow fails

| Where | Likely cause |
|---|---|
| "Add the Spritz patches to the series" | Upstream added a patch with the same number as one of ours (0920–0924). Renumber ours in `wine-patches/` |
| "Build Wine with the series", `patch does not apply` | Upstream changed `dlls/ws2_32/socket.c`, `dlls/crypt32/msg.c` or `dlls/kernelbase/debug.c` around our patches. The patch needs porting again |
| "Build Wine with the series", compile error in `programs/steam` | Proton's stub (`0924`) no longer builds against upstream Wine; port it or drop it and `0923` still works on its own |
| "Build Wine with the series", configure or compile | Download the **wine-build-logs** artifact. A missing library shows in `configure.log`; add its `-dev` package to the workflow's `apt-get install` |
| "Swap the patched modules into the app" | The release layout changed, or the patched DLL lacks the `PW_` switches, meaning a patch did not take |
| "Build the game prefixes", `test -s` | A prefix has no `system32/steam.exe`: `0924` did not build it |
| "Build the game prefixes" | A recipe step failed; `pw_install.py` names the step. A download (DXVK, Liberation fonts) may also have been unavailable |
