/* Host self-test against live data that `make check` downloads:
 *   selftest branches.json build.json game.manifest.zst chunk.zst
 * chunk.zst is the first chunk of the manifest's first file; without it the
 * test prints that chunk's URL ("chunk-url: ...") for check.sh to fetch. */
#include "sophon.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static char *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    char *b;
    assert(f);
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(*n + 1);
    assert(b && fread(b, 1, *n, f) == *n);
    b[*n] = 0;
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    size_t n, count, zn;
    char tag[32], out[260], hex[33];
    SophonBranch br;
    SophonCategory cats[256], *game = NULL;
    SophonManifest m;
    int64_t sum = 0;
    size_t dirs = 0, files = 0;
    char *json, *z;
    uint8_t *raw, digest[16];

    /* selftest <biz> <branches.json> <build.json> <game manifest> [<first chunk>] */
    assert(argc == 5 || argc == 6);
    /* paths: network input */
    assert(!sophon_safe_path("a/b\\c.dat", out, sizeof out) && !strcmp(out, "a\\b\\c.dat"));
    assert(!sophon_safe_path("./a//b", out, sizeof out) && !strcmp(out, "a\\b"));
    assert(sophon_safe_path("../x", out, sizeof out) && sophon_safe_path("a/../x", out, sizeof out));
    assert(sophon_safe_path("/etc", out, sizeof out) && sophon_safe_path("C:x", out, sizeof out));
    assert(sophon_safe_path("", out, sizeof out) && sophon_safe_path(".", out, sizeof out));
    assert(sophon_safe_path("abcdef", out, 4));
    assert(sophon_safe_object("0123abcd_ffee") && !sophon_safe_object("a/b") && !sophon_safe_object(".."));

    json = slurp(argv[2], &n);
    assert(!sophon_find_branch(json, argv[1], 0, &br));
    assert(br.package_id[0] && br.password[0] && br.tag[0]);
    assert(sophon_find_branch(json, "no_such_game", 0, &br));
    printf("%s branch %s %s tag %s\n", argv[1], br.branch, br.package_id, br.tag);
    free(json);

    json = slurp(argv[3], &n);
    assert(!sophon_parse_build(json, tag, cats, 256, &count));
    for (size_t i = 0; i < count; i++)
        if (!strcmp(cats[i].field, "game")) game = &cats[i];
    assert(game && game->manifest_zstd && game->chunk_zstd);
    printf("%zu categories\n", count);
    assert(sophon_parse_build("{\"data\":null,\"retcode\":-1}", tag, cats, 16, &count));
    free(json);

    z = slurp(argv[4], &zn);
    raw = malloc((size_t)game->manifest_usize);
    assert(!sophon_unzstd(z, zn, raw, (size_t)game->manifest_usize));
    assert(sophon_unzstd(z, zn - 1, raw, (size_t)game->manifest_usize)); /* truncated */
    assert(!sophon_parse_manifest(raw, (size_t)game->manifest_usize, &m));
    for (size_t i = 0; i < m.count; i++) {
        assert(!sophon_safe_path(m.assets[i].name, out, sizeof out));
        if (sophon_is_dir(&m.assets[i])) { dirs++; continue; }
        files++;
        sum += m.assets[i].size;
    }
    printf("manifest %zu files %zu dirs, %lld bytes (stats %lld)\n", files, dirs, (long long)sum,
           (long long)game->total_usize);
    assert(sum == game->total_usize);
    /* A truncated manifest must fail, not yield a partial file list. */
    {
        SophonManifest bad;
        assert(sophon_parse_manifest(raw, (size_t)game->manifest_usize - 3, &bad));
    }

    if (argc == 5) {
        printf("chunk-url: %s/%s\n", game->chunk_url, m.assets[0].chunks[0].name);
    } else {
        const SophonChunk *c = &m.assets[0].chunks[0];
        char *cz = slurp(argv[5], &zn);
        uint8_t *u = malloc((size_t)c->usize);
        assert((int64_t)zn == c->size);
        assert(!sophon_unzstd(cz, zn, u, (size_t)c->usize));
        CC_MD5(u, (CC_LONG)c->usize, digest);
        for (int i = 0; i < 16; i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
        assert(!strcmp(hex, c->md5));
        printf("chunk %s md5 ok\n", c->name);
        free(u);
        free(cz);
    }
    sophon_free_manifest(&m);
    free(raw);
    free(z);
    puts("selftest ok");
    return 0;
}
