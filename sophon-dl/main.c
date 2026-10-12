/* sophon-dl: installs, resumes and repairs a HoYoverse game through the
 * Sophon chunk protocol, inside prospero-win's Wine on the PS5.
 *
 *   sophon-dl.exe <biz> <install dir> <matching field>... [--verify] [--predownload]
 *   sophon-dl.exe hk4e_global C:\Games\GenshinImpact game en-us
 *
 * A GUI program on purpose: a console program would need conhost.exe, a
 * second process, which Wine on the PS5 cannot start. Progress is drawn with
 * GDI and logged to <install dir>\sophon-dl.log. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sophon.h"

/* advapi32's MD5 is plain PE code in Wine; bcrypt would want a Unix library
 * the PS5 build does not ship. */
typedef struct { ULONG i[2]; ULONG buf[4]; unsigned char in[64]; unsigned char digest[16]; } MD5_CTX;
void WINAPI MD5Init(MD5_CTX *);
void WINAPI MD5Update(MD5_CTX *, const unsigned char *, unsigned int);
void WINAPI MD5Final(MD5_CTX *);

/* ZZZ alone offers ~130 build categories (game, voices, content packs); a run picks a few. */
enum { WORKERS = 8, TRIES = 10, MAX_CATS = 256, MAX_FIELDS = 16 };

typedef struct { const SophonAsset *a; const char *chunk_url; int zstd; } Job;

static struct {
    volatile LONG64 done, total, net;
    volatile LONG files_done, files_total, failed;
    volatile LONG finished;   /* 1 ok, -1 error */
    WCHAR head[256], status[512], error[512];
} g;
static CRITICAL_SECTION lock;
static HINTERNET session;
static HWND win;
static FILE *logf;

static WCHAR root[MAX_PATH];
static char biz[64], fields[MAX_FIELDS][32];
static int nfields, verify, predownload, trust_sizes;
static Job *jobs;
static LONG njobs, next_job;

static void logv(WCHAR *dst, const char *fmt, va_list ap)
{
    char buf[512];
    vsnprintf(buf, sizeof buf, fmt, ap);
    EnterCriticalSection(&lock);
    if (dst) MultiByteToWideChar(CP_UTF8, 0, buf, -1, dst, 512);
    if (logf) { fprintf(logf, "%s\n", buf); fflush(logf); }
    LeaveCriticalSection(&lock);
}
static void say(const char *fmt, ...) { va_list ap; va_start(ap, fmt); logv(g.status, fmt, ap); va_end(ap); }
static void warn(const char *fmt, ...) { va_list ap; va_start(ap, fmt); logv(NULL, fmt, ap); va_end(ap); }
static int fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    logv(g.error, fmt, ap);
    va_end(ap);
    return -1;
}

/* ---- HTTP ---- */

/* One GET into a NUL-terminated buffer; expect > 0 requires that exact size. */
static char *get_once(const char *url, size_t expect, size_t *len)
{
    WCHAR wurl[2048], host[256];
    URL_COMPONENTS uc = { sizeof uc };
    HINTERNET conn = NULL, req = NULL;
    DWORD code = 0, sz = sizeof code, got;
    /* expect + 2: room to see that nothing follows the expected bytes */
    size_t n = 0, cap = expect ? expect + 2 : 1 << 16;
    char *buf = NULL, *p;

    if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048)) return NULL;
    uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS ||
        uc.dwHostNameLength >= 256)
        return NULL;
    memcpy(host, uc.lpszHostName, uc.dwHostNameLength * sizeof *host);
    host[uc.dwHostNameLength] = 0;
    /* lpszUrlPath points into wurl, so it runs on through the query string. */
    if (!(conn = WinHttpConnect(session, host, uc.nPort, 0)) ||
        !(req = WinHttpOpenRequest(conn, L"GET", uc.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) ||
        !WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req, NULL) ||
        !WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX) ||
        code != 200) {
        warn("GET %s: HTTP %lu, error %lu", url, code, GetLastError());
        goto out;
    }
    if (!(buf = malloc(cap))) goto out;
    for (;;) {
        if (n + 1 == cap) {
            if (expect || cap > (64u << 20) || !(p = realloc(buf, cap * 2))) goto bad;
            buf = p;
            cap *= 2;
        }
        if (!WinHttpReadData(req, buf + n, (DWORD)(cap - n - 1), &got)) goto bad;
        if (!got) break;
        n += got;
        InterlockedExchangeAdd64(&g.net, got);
    }
    if (expect && n != expect) goto bad;
    buf[n] = 0;
    *len = n;
    goto out;
bad:
    warn("GET %s: short or failed read (%lu bytes, error %lu)", url, (unsigned long)n, GetLastError());
    free(buf);
    buf = NULL;
out:
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    return buf;
}

static char *get(const char *url, size_t expect, size_t *len)
{
    for (int i = 0; i < TRIES; i++) {
        char *b = get_once(url, expect, len);
        if (b) return b;
        Sleep(1000);
    }
    return NULL;
}

/* ---- files ---- */

static void md5_hex(const void *p, size_t n, char hex[33])
{
    MD5_CTX c;
    MD5Init(&c);
    MD5Update(&c, p, (unsigned)n);
    MD5Final(&c);
    for (int i = 0; i < 16; i++) snprintf(hex + 2 * i, 3, "%02x", c.digest[i]);
}

/* Create every directory on the way to path, and path itself if last. */
static int make_dirs(WCHAR *path, int last)
{
    for (WCHAR *p = path + 3; ; p++) {   /* past "C:\" */
        if (*p == '\\' || (!*p && last)) {
            WCHAR c = *p;
            *p = 0;
            if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                *p = c;
                return -1;
            }
            *p = c;
        }
        if (!*p) return 0;
    }
}

static int64_t file_size(const WCHAR *path)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &d)) return -1;
    return (int64_t)d.nFileSizeHigh << 32 | d.nFileSizeLow;
}

static int at_offset(HANDLE h, int write, void *buf, int64_t off, DWORD n)
{
    OVERLAPPED ov = { 0 };
    DWORD done;
    ov.Offset = (DWORD)off;
    ov.OffsetHigh = (DWORD)(off >> 32);
    return (write ? WriteFile(h, buf, n, &done, &ov) : ReadFile(h, buf, n, &done, &ov)) && done == n ? 0 : -1;
}

static int set_size(HANDLE h, int64_t size)
{
    LARGE_INTEGER li;
    li.QuadPart = size;
    return SetFilePointerEx(h, li, NULL, FILE_BEGIN) && SetEndOfFile(h) ? 0 : -1;
}

/* Fetch one chunk into u (usize bytes) and check its MD5. */
static int fetch_chunk(const Job *j, const SophonChunk *c, uint8_t *u, int zstd)
{
    char url[1024], hex[33];
    snprintf(url, sizeof url, "%s/%s", j->chunk_url, c->name);
    for (int i = 0; i < 3; i++) {
        size_t n;
        char *z = get(url, (size_t)c->size, &n);
        int ok;
        if (!z) return -1;
        ok = zstd ? !sophon_unzstd(z, n, u, (size_t)c->usize)
                  : (c->size == c->usize && (memcpy(u, z, n), 1));
        free(z);
        if (ok) {
            md5_hex(u, (size_t)c->usize, hex);
            if (!_stricmp(hex, c->md5)) return 0;
        }
        warn("chunk %s: bad data, fetching again", c->name);
    }
    return -1;
}

/* Install one file: work in <path>_tempSophon, keep every chunk whose bytes
 * already hash right (this is resume and repair), then rename into place. */
static int do_asset(const Job *j, int zstd)
{
    const SophonAsset *a = j->a;
    char rel[MAX_PATH], hex[33];
    WCHAR wrel[MAX_PATH], path[2 * MAX_PATH], tmp[2 * MAX_PATH + 16];
    HANDLE h;
    LARGE_INTEGER cur;
    int rc = 0;

    if (sophon_safe_path(a->name, rel, sizeof rel) ||
        !MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, MAX_PATH))
        return fail("unsafe path %s", a->name);
    swprintf(path, 2 * MAX_PATH, L"%ls\\%ls", root, wrel);
    if (sophon_is_dir(a)) return make_dirs(path, 1) ? fail("cannot create folder %s", rel) : 0;
    if (make_dirs(path, 0)) return fail("cannot create folder for %s", rel);
    swprintf(tmp, sizeof tmp / sizeof *tmp, L"%ls_tempSophon", path);

    if (trust_sizes && file_size(path) == a->size && file_size(tmp) < 0) {
        InterlockedExchangeAdd64(&g.done, a->size);
        return 0;
    }
    if (file_size(tmp) < 0 && file_size(path) >= 0 && !MoveFileW(path, tmp))
        return fail("cannot move %s aside (error %lu)", rel, GetLastError());
    h = CreateFileW(tmp, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return fail("cannot open %s (error %lu)", rel, GetLastError());
    if (!GetFileSizeEx(h, &cur)) cur.QuadPart = 0;
    if (cur.QuadPart > a->size && !set_size(h, a->size)) cur.QuadPart = a->size;

    for (size_t i = 0; i < a->nchunks && !rc; i++) {
        const SophonChunk *c = &a->chunks[i];
        uint8_t *u = malloc((size_t)c->usize);
        int have = 0;
        if (!u) { rc = fail("out of memory"); break; }
        if (cur.QuadPart >= c->offset + c->usize && !at_offset(h, 0, u, c->offset, (DWORD)c->usize)) {
            md5_hex(u, (size_t)c->usize, hex);
            have = !_stricmp(hex, c->md5);
        }
        if (!have) {
            if (fetch_chunk(j, c, u, zstd)) rc = fail("cannot download %s of %s", c->name, rel);
            else if (at_offset(h, 1, u, c->offset, (DWORD)c->usize))
                rc = fail("cannot write %s (error %lu) - is the disk full?", rel, GetLastError());
        }
        free(u);
        if (!rc) InterlockedExchangeAdd64(&g.done, c->usize);
    }
    if (!rc && set_size(h, a->size)) rc = fail("cannot size %s", rel);
    CloseHandle(h);
    if (!rc && !MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING))
        rc = fail("cannot rename %s into place (error %lu)", rel, GetLastError());
    return rc;
}

static DWORD WINAPI worker(void *arg)
{
    LONG i;
    (void)arg;
    while ((i = InterlockedIncrement(&next_job) - 1) < njobs) {
        if (do_asset(&jobs[i], jobs[i].zstd)) InterlockedIncrement(&g.failed);
        InterlockedIncrement(&g.files_done);
    }
    return 0;
}

/* ---- the run ---- */


static int read_tag(const WCHAR *path, char tag[32])
{
    FILE *f = _wfopen(path, L"rb");
    size_t n;
    if (!f) return -1;
    n = fread(tag, 1, 31, f);
    fclose(f);
    tag[n] = 0;
    tag[strcspn(tag, "\r\n")] = 0;
    return 0;
}

static int write_tag(const WCHAR *path, const char *tag)
{
    FILE *f = _wfopen(path, L"wb");
    if (!f) return -1;
    fprintf(f, "%s\n", tag);
    return fclose(f);
}

static int run(void)
{
    static SophonCategory cats[MAX_CATS];
    size_t ncats;
    char url[2048], tag[32], installed[32];
    WCHAR vpath[MAX_PATH + 32], heads[256];
    SophonBranch br;
    SophonManifest mf[MAX_FIELDS];
    SophonCategory *sel[MAX_FIELDS];
    HANDLE threads[WORKERS];
    ULARGE_INTEGER avail;
    int64_t have = 0;
    size_t n;
    char *json;
    int have_tag;

    say("Looking up %s", biz);
    snprintf(url, sizeof url, "%s/getGameBranches?launcher_id=%s", SOPHON_LAUNCHER_API, SOPHON_LAUNCHER_ID);
    if (!(json = get(url, 0, &n))) return fail("cannot reach HoYoverse (getGameBranches); check the network");
    if (sophon_find_branch(json, biz, predownload, &br)) {
        free(json);
        return fail(predownload ? "%s has no pre-download right now" : "unknown game %s", biz);
    }
    free(json);
    if (!sophon_safe_object(br.branch) || !sophon_safe_object(br.package_id) ||
        !sophon_safe_object(br.password) || !sophon_safe_object(br.tag))
        return fail("unexpected characters in the branch info");

    snprintf(url, sizeof url, "%s/getBuild?branch=%s&package_id=%s&password=%s&tag=%s",
             SOPHON_CHUNK_API, br.branch, br.package_id, br.password, br.tag);
    if (!(json = get(url, 0, &n))) return fail("cannot fetch the build (getBuild)");
    if (sophon_parse_build(json, tag, cats, MAX_CATS, &ncats)) {
        warn("getBuild said: %.300s", json);
        free(json);
        return fail("unexpected getBuild answer; see sophon-dl.log");
    }
    free(json);

    for (int i = 0; i < nfields; i++) {
        sel[i] = NULL;
        for (size_t k = 0; k < ncats; k++)
            if (!strcmp(cats[k].field, fields[i])) sel[i] = &cats[k];
        if (!sel[i]) return fail("%s %s does not offer \"%s\"", biz, tag, fields[i]);
    }
    swprintf(heads, 256, L"%hs %hs:", biz, tag);
    /* The heading only names the fields that fit (each takes at most 33 WCHARs). */
    for (int i = 0; i < nfields && wcslen(heads) + 33 < 256; i++) {
        wcscat(heads, L" ");
        MultiByteToWideChar(CP_UTF8, 0, fields[i], -1, heads + wcslen(heads), 32);
    }
    EnterCriticalSection(&lock);
    wcscpy(g.head, heads);
    LeaveCriticalSection(&lock);

    for (int i = 0; i < nfields; i++) {
        SophonCategory *c = sel[i];
        uint8_t *raw;
        say("Reading the %s file list", c->field);
        snprintf(url, sizeof url, "%s/%s", c->manifest_url, c->manifest_id);
        if (!(json = get(url, 0, &n))) return fail("cannot fetch the %s manifest", c->field);
        if (c->manifest_zstd) {
            if (!(raw = malloc((size_t)c->manifest_usize)) ||
                sophon_unzstd(json, n, raw, (size_t)c->manifest_usize))
                return fail("cannot unpack the %s manifest", c->field);
            free(json);
        } else {
            raw = (uint8_t *)json;
            c->manifest_usize = (int64_t)n;
        }
        if (sophon_parse_manifest(raw, (size_t)c->manifest_usize, &mf[i]))
            return fail("cannot read the %s manifest", c->field);
        free(raw);
        njobs += (LONG)mf[i].count;
    }

    if (!(jobs = calloc((size_t)njobs, sizeof *jobs))) return fail("out of memory");
    njobs = 0;
    for (int i = 0; i < nfields; i++)
        for (size_t k = 0; k < mf[i].count; k++) {
            char rel[MAX_PATH];
            WCHAR wrel[MAX_PATH], p[2 * MAX_PATH];
            const SophonAsset *a = &mf[i].assets[k];
            int64_t size;
            if (sophon_safe_path(a->name, rel, sizeof rel)) return fail("unsafe path in manifest: %s", a->name);
            jobs[njobs].a = a;
            jobs[njobs].chunk_url = sel[i]->chunk_url;
            jobs[njobs++].zstd = sel[i]->chunk_zstd;
            if (sophon_is_dir(a)) continue;
            g.total += a->size;
            MultiByteToWideChar(CP_UTF8, 0, rel, -1, wrel, MAX_PATH);
            swprintf(p, 2 * MAX_PATH, L"%ls\\%ls", root, wrel);
            size = file_size(p);
            wcscat(p, L"_tempSophon");
            if (file_size(p) > size) size = file_size(p);
            if (size > 0) have += size;
        }
    g.files_total = njobs;

    if (make_dirs(root, 1)) return fail("cannot create %ls", root);
    /* Only trust a non-zero answer: the console's statfs support is thin. */
    if (GetDiskFreeSpaceExW(root, &avail, NULL, NULL) && avail.QuadPart &&
        (int64_t)avail.QuadPart < g.total - have)
        return fail("not enough space: need %.1f GB more, %.1f GB free",
                    (g.total - have) / 1e9, avail.QuadPart / 1e9);

    /* The tag file names the version every completed file belongs to; only then
     * may a file of the right size be trusted without hashing it. */
    swprintf(vpath, MAX_PATH + 32, L"%ls\\sophon_version.txt", root);
    have_tag = !read_tag(vpath, installed);
    trust_sizes = !verify && (!have_tag || !strcmp(installed, tag));
    if (!have_tag && write_tag(vpath, tag)) return fail("cannot write %ls", vpath);
    say(trust_sizes ? "Downloading" : "Checking existing files against %s, then downloading", tag);

    for (int i = 0; i < WORKERS; i++) threads[i] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    WaitForMultipleObjects(WORKERS, threads, TRUE, INFINITE);
    if (g.failed) return fail("%ld files failed; start the downloader again to resume", g.failed);
    if (write_tag(vpath, tag)) return fail("cannot write %ls", vpath);
    say("Done: %s %s is installed", biz, tag);
    return 0;
}

static DWORD WINAPI run_thread(void *arg)
{
    (void)arg;
    InterlockedExchange(&g.finished, run() ? -1 : 1);
    return 0;
}

/* ---- window ---- */

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    static LONG64 last_net;
    static double speed;
    static HFONT font;
    switch (msg) {
    case WM_CREATE:
        font = CreateFontW(40, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY,
                           0, L"Tahoma");
        SetTimer(h, 1, 1000, NULL);
        return 0;
    case WM_TIMER: {
        LONG64 net = g.net;
        if (wp == 2) { DestroyWindow(h); return 0; }
        speed = (net - last_net) / 1e6;
        last_net = net;
        if (g.finished == 1) KillTimer(h, 1), SetTimer(h, 2, 10000, NULL);
        InvalidateRect(h, NULL, TRUE);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        if (g.finished) DestroyWindow(h);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        WCHAR text[2048];
        RECT r;
        HDC dc = BeginPaint(h, &ps);
        GetClientRect(h, &r);
        FillRect(dc, &r, GetStockObject(BLACK_BRUSH));
        InflateRect(&r, -80, -80);
        SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(230, 230, 230));
        EnterCriticalSection(&lock);
        swprintf(text, 2048,
                 L"%ls\n%ls\n\nFiles: %ld / %ld\nData: %.2f / %.2f GB\nDownload: %.1f MB/s\n\n%ls\n%ls%ls",
                 g.head, root, g.files_done, g.files_total, g.done / 1e9, g.total / 1e9, speed, g.status,
                 g.error, g.finished == 1 ? L"Closing in a few seconds."
                          : g.finished ? L"\nPress Cross to close." : L"");
        LeaveCriticalSection(&lock);
        DrawTextW(dc, text, -1, &r, DT_LEFT | DT_WORDBREAK);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    WNDCLASSW wc = { 0 };
    WCHAR logpath[MAX_PATH + 32];
    int argc;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    MSG m;
    (void)prev; (void)cmd; (void)show;

    InitializeCriticalSection(&lock);
    wcscpy(g.head, L"Sophon downloader");
    for (int i = 3; argv && i < argc; i++) {
        if (!wcscmp(argv[i], L"--verify")) verify = 1;
        else if (!wcscmp(argv[i], L"--predownload")) predownload = 1;
        else if (nfields < MAX_FIELDS) WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, fields[nfields++], 32, NULL, NULL);
    }
    if (!argv || argc < 4 || wcslen(argv[2]) < 4 || argv[2][1] != ':' || argv[2][2] != '\\' ||
        wcslen(argv[2]) >= MAX_PATH ||
        !WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, biz, sizeof biz, NULL, NULL)) {
        fail("usage: sophon-dl.exe <biz> <C:\\install dir> <matching field>... [--verify] [--predownload]");
        g.finished = -1;
    } else {
        wcscpy(root, argv[2]);
        while (root[wcslen(root) - 1] == '\\') root[wcslen(root) - 1] = 0;
        make_dirs(root, 1);
        swprintf(logpath, MAX_PATH + 32, L"%ls\\sophon-dl.log", root);
        logf = _wfopen(logpath, L"ab");
        session = WinHttpOpen(L"sophon-dl/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) {
            fail("WinHttpOpen failed (error %lu)", GetLastError());
            g.finished = -1;
        } else {
            WinHttpSetTimeouts(session, 15000, 15000, 20000, 20000);
            CloseHandle(CreateThread(NULL, 0, run_thread, NULL, 0, NULL));
        }
    }

    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = L"sophon-dl";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    win = CreateWindowExW(0, wc.lpszClassName, L"Sophon downloader", WS_POPUP | WS_VISIBLE, 0, 0,
                          GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, inst, NULL);
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    /* Leaving mid-download is fine: the next run resumes from the chunks on disk. */
    ExitProcess(g.finished == 1 ? 0 : 1);
}
