#!/bin/sh
# Fetch live API data, each game's manifest and one chunk, and run selftest on them.
set -eu
cd "$(dirname "$0")"
mkdir -p live
curl -fsS -o live/branches.json \
    'https://sg-hyp-api.hoyoverse.com/hyp/hyp-connect/api/getGameBranches?launcher_id=VYTpXlbWo8'
for biz in hk4e_global hkrpg_global nap_global; do
    q=$(python3 -I -c '
import json, sys, urllib.parse
for g in json.load(open(sys.argv[1]))["data"]["game_branches"]:
    if g["game"]["biz"] == sys.argv[2]:
        m = g["main"]
        print(urllib.parse.urlencode({"branch": m["branch"], "package_id": m["package_id"],
                                      "password": m["password"], "tag": m["tag"]}))' live/branches.json $biz)
    curl -fsS -o live/$biz.build.json "https://sg-public-api.hoyoverse.com/downloader/sophon_chunk/api/getBuild?$q"
    url=$(python3 -I -c '
import json, sys
for m in json.load(open(sys.argv[1]))["data"]["manifests"]:
    if m["matching_field"] == "game":
        print(m["manifest_download"]["url_prefix"].rstrip("/") + "/" + m["manifest"]["id"])' live/$biz.build.json)
    curl -fsS -o live/$biz.manifest.zst "$url"
    chunk=$(./selftest $biz live/branches.json live/$biz.build.json live/$biz.manifest.zst | sed -n 's/^chunk-url: //p')
    curl -fsS -o live/$biz.chunk.zst "$chunk"
    ./selftest $biz live/branches.json live/$biz.build.json live/$biz.manifest.zst live/$biz.chunk.zst
done
