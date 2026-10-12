/* Sophon protocol core: API JSON, manifest protobuf and path checks.
 * Portable C (no Win32) so selftest.c can run it on the build machine.
 * Protocol notes: HoyoFetch's sophon-protocol.md. */
#ifndef SOPHON_H
#define SOPHON_H

#include <stddef.h>
#include <stdint.h>

#define SOPHON_LAUNCHER_API "https://sg-hyp-api.hoyoverse.com/hyp/hyp-connect/api"
#define SOPHON_CHUNK_API "https://sg-public-api.hoyoverse.com/downloader/sophon_chunk/api"
#define SOPHON_LAUNCHER_ID "VYTpXlbWo8" /* global HoYoPlay */

typedef struct SophonChunk {
    char *name;   /* CDN object name, checked by sophon_safe_object */
    char *md5;    /* MD5 of the decompressed chunk, hex */
    int64_t offset, size, usize;
} SophonChunk;

typedef struct SophonAsset {
    char *name;   /* as sent; join it only after sophon_safe_path */
    char *md5;
    int type;
    int64_t size;
    SophonChunk *chunks;
    size_t nchunks;
} SophonAsset;

typedef struct SophonManifest {
    SophonAsset *assets;
    size_t count;
} SophonManifest;

typedef struct SophonBranch {
    char package_id[64], password[64], branch[32], tag[32];
} SophonBranch;

typedef struct SophonCategory {
    char field[32];
    char manifest_id[128];
    char manifest_url[512];  /* url_prefix, no trailing slash */
    char chunk_url[512];
    int manifest_zstd, chunk_zstd;
    int64_t manifest_usize;  /* decompressed manifest size */
    int64_t total_usize;     /* stats.uncompressed_size */
} SophonCategory;

/* 0 on success. predownload selects "pre_download" instead of "main". */
int sophon_find_branch(const char *json, const char *biz, int predownload, SophonBranch *out);

/* getBuild response: fills up to cap categories, *count is the number found. */
int sophon_parse_build(const char *json, char tag[32], SophonCategory *out, size_t cap, size_t *count);

/* Decoded (decompressed) SophonManifestProto. 0 on success. */
int sophon_parse_manifest(const uint8_t *buf, size_t len, SophonManifest *out);
void sophon_free_manifest(SophonManifest *m);

/* Directory entry per the protocol: AssetType != 0 or no MD5. */
int sophon_is_dir(const SophonAsset *a);

/* A relative path with backslashes, or -1 for absolute paths, drive letters,
 * ".." components, control characters or overflow. Network input. */
int sophon_safe_path(const char *name, char *out, size_t cap);

/* Names that go into a URL path: [A-Za-z0-9_.-] only, non-empty. */
int sophon_safe_object(const char *name);

/* Decompress one zstd frame of known size; 0 when exactly usize bytes came out. */
int sophon_unzstd(const void *src, size_t n, void *dst, size_t usize);

#endif
