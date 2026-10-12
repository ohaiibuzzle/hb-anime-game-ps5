#!/bin/sh
# Add a game's entry to the console's launcher, beside the "Download / Update"
# entry that pw_prefix.py push added; both use the <game> prefix.
# Usage: push-game-profile.sh <genshin-impact|honkai-star-rail|zenless-zone-zero> <PS5 IP> [FTP port]
set -eu
usage="usage: $0 <genshin-impact|honkai-star-rail|zenless-zone-zero> <PS5 IP> [FTP port]"
game=${1:?$usage}
host=${2:?$usage}
profile=$game-game.profile
src=$(dirname "$0")/profiles/$profile
[ -f "$src" ] || { echo "no $src; $usage" >&2; exit 2; }
base=ftp://$host:${3:-2121}/data/prospero-win/profiles
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
curl -fsS -T "$src" "$base/"
# The launcher shows profiles.lst's order when the file exists, and only what it lists.
if curl -fsS -o "$tmp" "$base/profiles.lst"; then
    grep -qx "$profile" "$tmp" ||
        { echo "$profile" >> "$tmp" && curl -fsS -T "$tmp" "$base/profiles.lst"; }
fi
echo "added $profile to the launcher"
