#include "sophon.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "zstd.h"

/* ---- JSON ---- */

static const cJSON *at(const cJSON *o, const char *k) { return cJSON_GetObjectItemCaseSensitive(o, k); }

/* Numbers arrive as strings ("4862327") or numbers. */
static int64_t num(const cJSON *v)
{
    if (cJSON_IsString(v)) return strtoll(v->valuestring, NULL, 10);
    if (cJSON_IsNumber(v)) return (int64_t)v->valuedouble;
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
    return 0;
}

static int copy(char *dst, size_t cap, const cJSON *v)
{
    size_t n;
    if (!cJSON_IsString(v)) return -1;
    n = strlen(v->valuestring);
    if (n >= cap) return -1;
    memcpy(dst, v->valuestring, n + 1);
    return 0;
}

static int https_prefix(char *dst, size_t cap, const cJSON *v)
{
    size_t n;
    if (copy(dst, cap, v) || strncmp(dst, "https://", 8)) return -1;
    n = strlen(dst);
    while (n && dst[n - 1] == '/') dst[--n] = 0;
    return 0;
}

int sophon_find_branch(const char *json, const char *biz, int predownload, SophonBranch *out)
{
    cJSON *root = cJSON_Parse(json);
    const cJSON *g;
    int rc = -1;

    cJSON_ArrayForEach(g, at(at(root, "data"), "game_branches")) {
        const cJSON *b;
        if (!cJSON_IsString(at(at(g, "game"), "biz")) || strcmp(at(at(g, "game"), "biz")->valuestring, biz))
            continue;
        b = at(g, predownload ? "pre_download" : "main");
        if (cJSON_IsObject(b) && !copy(out->package_id, sizeof out->package_id, at(b, "package_id")) &&
            !copy(out->password, sizeof out->password, at(b, "password")) &&
            !copy(out->branch, sizeof out->branch, at(b, "branch")) &&
            !copy(out->tag, sizeof out->tag, at(b, "tag")))
            rc = 0;
        break;
    }
    cJSON_Delete(root);
    return rc;
}

int sophon_parse_build(const char *json, char tag[32], SophonCategory *out, size_t cap, size_t *count)
{
    cJSON *root = cJSON_Parse(json);
    const cJSON *data = at(root, "data"), *m;
    int rc = -1;

    *count = 0;
    if (!cJSON_IsObject(data) || copy(tag, 32, at(data, "tag"))) goto done;
    cJSON_ArrayForEach(m, at(data, "manifests")) {
        SophonCategory *c = &out[*count];
        const cJSON *md = at(m, "manifest_download"), *cd = at(m, "chunk_download");
        if (*count == cap) goto done;
        /* Nothing documents encrypted downloads; refuse rather than write garbage. */
        if (num(at(md, "encryption")) || num(at(cd, "encryption"))) goto done;
        if (copy(c->field, sizeof c->field, at(m, "matching_field")) ||
            copy(c->manifest_id, sizeof c->manifest_id, at(at(m, "manifest"), "id")) ||
            !sophon_safe_object(c->manifest_id) ||
            https_prefix(c->manifest_url, sizeof c->manifest_url, at(md, "url_prefix")) ||
            https_prefix(c->chunk_url, sizeof c->chunk_url, at(cd, "url_prefix")))
            goto done;
        c->manifest_zstd = num(at(md, "compression")) != 0;
        c->chunk_zstd = num(at(cd, "compression")) != 0;
        c->manifest_usize = num(at(at(m, "manifest"), "uncompressed_size"));
        c->total_usize = num(at(at(m, "stats"), "uncompressed_size"));
        if (c->manifest_usize <= 0) goto done;
        (*count)++;
    }
    rc = *count ? 0 : -1;
done:
    cJSON_Delete(root);
    return rc;
}

/* ---- protobuf ---- */

typedef struct { const uint8_t *p, *end; } Rd;

static int varint(Rd *r, uint64_t *v)
{
    *v = 0;
    for (int s = 0; s < 64 && r->p < r->end; s += 7) {
        uint8_t b = *r->p++;
        *v |= (uint64_t)(b & 0x7f) << s;
        if (!(b & 0x80)) return 0;
    }
    return -1;
}

/* Next field: its number, wire type, and for wire type 2 the sub-reader. */
static int field(Rd *r, unsigned *num_, unsigned *wt, uint64_t *v, Rd *sub)
{
    uint64_t tag;
    if (varint(r, &tag)) return -1;
    *num_ = (unsigned)(tag >> 3);
    *wt = tag & 7;
    switch (*wt) {
    case 0: return varint(r, v);
    case 1: if (r->end - r->p < 8) return -1; r->p += 8; return 0;
    case 5: if (r->end - r->p < 4) return -1; r->p += 4; return 0;
    case 2:
        if (varint(r, v) || *v > (uint64_t)(r->end - r->p)) return -1;
        sub->p = r->p;
        sub->end = r->p + *v;
        r->p += *v;
        return 0;
    }
    return -1;
}

static char *dup(const Rd *s)
{
    size_t n = (size_t)(s->end - s->p);
    char *c = malloc(n + 1);
    if (!c) return NULL;
    memcpy(c, s->p, n);
    c[n] = 0;
    return c;
}

static int grow(void **arr, size_t n, size_t *cap, size_t elem)
{
    void *p;
    if (n < *cap) return 0;
    *cap = *cap ? *cap * 2 : 16;
    if (!(p = realloc(*arr, *cap * elem))) return -1;
    *arr = p;
    return 0;
}

static int parse_chunk(Rd r, SophonChunk *c)
{
    unsigned n, wt;
    uint64_t v;
    Rd s;
    memset(c, 0, sizeof *c);
    while (r.p < r.end) {
        if (field(&r, &n, &wt, &v, &s)) return -1;
        if (n == 1 && wt == 2) { free(c->name); if (!(c->name = dup(&s))) return -1; }
        else if (n == 2 && wt == 2) { free(c->md5); if (!(c->md5 = dup(&s))) return -1; }
        else if (n == 3 && wt == 0) c->offset = (int64_t)v;
        else if (n == 4 && wt == 0) c->size = (int64_t)v;
        else if (n == 5 && wt == 0) c->usize = (int64_t)v;
    }
    if (!c->name || !c->md5 || !sophon_safe_object(c->name) || c->offset < 0 || c->size <= 0 ||
        c->usize <= 0 || c->usize > (1 << 30) || c->size > (1 << 30))
        return -1;
    return 0;
}

static void free_asset(SophonAsset *a)
{
    for (size_t i = 0; i < a->nchunks; i++) { free(a->chunks[i].name); free(a->chunks[i].md5); }
    free(a->chunks);
    free(a->name);
    free(a->md5);
}

static int parse_asset(Rd r, SophonAsset *a)
{
    unsigned n, wt;
    uint64_t v;
    Rd s;
    size_t cap = 0;
    memset(a, 0, sizeof *a);
    while (r.p < r.end) {
        if (field(&r, &n, &wt, &v, &s)) return -1;
        if (n == 1 && wt == 2) { free(a->name); if (!(a->name = dup(&s))) return -1; }
        else if (n == 2 && wt == 2) {
            if (grow((void **)&a->chunks, a->nchunks, &cap, sizeof *a->chunks)) return -1;
            /* count it first so free_asset releases a half-parsed chunk */
            if (parse_chunk(s, &a->chunks[a->nchunks++])) return -1;
        }
        else if (n == 3 && wt == 0) a->type = (int)v;
        else if (n == 4 && wt == 0) a->size = (int64_t)v;
        else if (n == 5 && wt == 2) { free(a->md5); if (!(a->md5 = dup(&s))) return -1; }
    }
    if (!a->name || a->size < 0) return -1;
    for (size_t i = 0; i < a->nchunks; i++)
        if (a->chunks[i].offset + a->chunks[i].usize > a->size) return -1;
    return 0;
}

int sophon_parse_manifest(const uint8_t *buf, size_t len, SophonManifest *out)
{
    Rd r = { buf, buf + len }, s;
    unsigned n, wt;
    uint64_t v;
    size_t cap = 0;

    memset(out, 0, sizeof *out);
    while (r.p < r.end) {
        if (field(&r, &n, &wt, &v, &s)) goto fail;
        if (n != 1 || wt != 2) continue;
        if (grow((void **)&out->assets, out->count, &cap, sizeof *out->assets)) goto fail;
        if (parse_asset(s, &out->assets[out->count++])) goto fail;
    }
    return 0;
fail:
    sophon_free_manifest(out);
    return -1;
}

void sophon_free_manifest(SophonManifest *m)
{
    for (size_t i = 0; i < m->count; i++) free_asset(&m->assets[i]);
    free(m->assets);
    memset(m, 0, sizeof *m);
}

int sophon_is_dir(const SophonAsset *a) { return a->type != 0 || !a->md5 || !a->md5[0]; }

/* ---- paths ---- */

int sophon_safe_path(const char *name, char *out, size_t cap)
{
    size_t o = 0;
    const char *seg = name;

    if (!name[0] || name[0] == '/' || name[0] == '\\') return -1;
    for (const char *p = name;; p++) {
        if (*p == '/' || *p == '\\' || !*p) {
            size_t len = (size_t)(p - seg);
            if (len == 2 && seg[0] == '.' && seg[1] == '.') return -1;
            if (len && !(len == 1 && seg[0] == '.')) {
                if (o + len + 2 > cap) return -1;
                if (o) out[o++] = '\\';
                memcpy(out + o, seg, len);
                o += len;
            }
            if (!*p) break;
            seg = p + 1;
        } else if ((unsigned char)*p < 0x20 || *p == ':') {
            return -1;
        }
    }
    if (!o) return -1;
    out[o] = 0;
    return 0;
}

int sophon_safe_object(const char *name)
{
    if (!name[0]) return 0;
    for (const char *p = name; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '_' || *p == '-' || *p == '.'))
            return 0;
    return strcmp(name, ".") && strcmp(name, "..");
}

int sophon_unzstd(const void *src, size_t n, void *dst, size_t usize)
{
    size_t r = ZSTD_decompress(dst, usize, src, n);
    return ZSTD_isError(r) || r != usize ? -1 : 0;
}
