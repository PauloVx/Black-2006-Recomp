/* The production manifest code runs against a private synthetic cache directory. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

enum { VSH_TOKEN_SIZE = 4, NV2A_MAX_TRANSFORM_PROGRAM_LENGTH = 136 };

typedef struct KGpuVs {
    uint32_t sh, len;
    uint32_t *words;
    uint32_t nwords;
} KGpuVs;

static char s_cache_dir[MAX_PATH];
static const char *kg_cache_dir(void) { return s_cache_dir; }
static void manifest_file(char out[600]);

static void *kg_read_file(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    long n;
    void *buf;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return NULL;
    }
    buf = n ? malloc((size_t)n) : NULL;
    if (n && (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n)) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = n;
    return buf;
}

#include "manifest.extracted.inc"

static unsigned s_checks, s_failures;
#define CHECK(expr, label) do { \
    ++s_checks; \
    if (!(expr)) { ++s_failures; printf("FAIL: %s\n", label); } \
} while (0)

static void reset_state(void)
{
    s_man_n = 0;
    s_man_loaded = false;
    memset(s_man_keys, 0, sizeof(s_man_keys));
}

static int set_case_dir(const char *root, const char *name)
{
    char path[600];
    DWORD attrs;
    if (snprintf(s_cache_dir, sizeof(s_cache_dir), "%s\\%s", root, name) >= (int)sizeof(s_cache_dir)) return 0;
    if (!CreateDirectoryA(s_cache_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return 0;
    attrs = GetFileAttributesA(s_cache_dir);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    manifest_file(path);
    if (!DeleteFileA(path) && GetLastError() != ERROR_FILE_NOT_FOUND) return 0;
    reset_state();
    return 1;
}

static void manifest_file(char out[600])
{
    snprintf(out, 600, "%s\\vs_manifest.bin", s_cache_dir);
}

static size_t record_bytes(uint8_t *out, uint32_t sh, uint32_t len,
                           const uint32_t *words, uint32_t nwords)
{
    const uint32_t hdr[4] = { KG_MAN_MAGIC, sh, len, nwords };
    const size_t bytes = sizeof(hdr) + (size_t)nwords * sizeof(words[0]);
    memcpy(out, hdr, sizeof(hdr));
    memcpy(out + sizeof(hdr), words, (size_t)nwords * sizeof(words[0]));
    return bytes;
}

static int write_bytes(const char *path, const void *data, size_t size, const char *mode)
{
    FILE *f = fopen(path, mode);
    int ok;
    if (!f) return 0;
    ok = fwrite(data, 1, size, f) == size && fclose(f) == 0;
    return ok;
}

static size_t read_bytes(const char *path, uint8_t *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return (size_t)-1;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) || (size_t)n > cap ||
        (n && fread(out, 1, (size_t)n, f) != (size_t)n)) {
        fclose(f);
        return (size_t)-1;
    }
    fclose(f);
    return (size_t)n;
}

static int file_equals(const char *path, const uint8_t *expected, size_t size)
{
    uint8_t got[8192];
    const size_t n = read_bytes(path, got, sizeof(got));
    return n == size && !memcmp(got, expected, size);
}

static KGpuVs entry(uint32_t sh, uint32_t len, uint32_t *words, uint32_t nwords)
{
    KGpuVs e;
    memset(&e, 0, sizeof(e));
    e.sh = sh;
    e.len = len;
    e.words = words;
    e.nwords = nwords;
    return e;
}

static int run_empty_and_duplicate(const char *root)
{
    uint32_t words_a[4] = { 0x01020304u, 0x11121314u, 0x21222324u, 0x31323334u };
    uint8_t expected[64];
    char path[600];
    KGpuVs a = entry(0x10203040u, 64u, words_a, 4u);
    size_t n;
    if (!set_case_dir(root, "empty-and-duplicate")) return 0;
    manifest_file(path);
    n = record_bytes(expected, a.sh, a.len, words_a, a.nwords);
    manifest_add(&a);
    CHECK(file_equals(path, expected, n), "empty manifest gets one complete record");
    CHECK(manifest_has(((uint64_t)a.sh << 32) | a.len), "successful append becomes a loaded key");
    manifest_add(&a);
    CHECK(file_equals(path, expected, n), "a duplicate key is not appended twice");
    return 1;
}

static int run_duplicate_existing(const char *root)
{
    uint32_t words_a[4] = { 0x41424344u, 0x51525354u, 0x61626364u, 0x71727374u };
    uint8_t expected[64];
    char path[600];
    KGpuVs a = entry(0xA0B0C0D0u, 80u, words_a, 4u);
    size_t n;
    if (!set_case_dir(root, "duplicate-existing")) return 0;
    manifest_file(path);
    n = record_bytes(expected, a.sh, a.len, words_a, a.nwords);
    CHECK(write_bytes(path, expected, n, "wb"), "write an existing complete record");
    manifest_add(&a);
    CHECK(file_equals(path, expected, n), "a loaded duplicate preserves the existing file byte for byte");
    return 1;
}

static int run_torn_tail(const char *root, int payload, int loaded_first)
{
    uint32_t words_a[4] = { 1u, 2u, 3u, 4u };
    uint32_t words_b[8] = { 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u };
    uint32_t words_c[4] = { 13u, 14u, 15u, 16u };
    uint8_t expected_a[64], expected_c[64], torn[64], final[128];
    uint32_t hdr_b[4] = { KG_MAN_MAGIC, 0x22222222u, 96u, 8u };
    char case_name[80], path[600];
    KGpuVs a = entry(0x11111111u, 32u, words_a, 4u);
    KGpuVs c = entry(0x33333333u, 128u, words_c, 4u);
    const size_t a_n = record_bytes(expected_a, a.sh, a.len, words_a, a.nwords);
    const size_t c_n = record_bytes(expected_c, c.sh, c.len, words_c, c.nwords);
    size_t torn_n, final_n;
    snprintf(case_name, sizeof(case_name), "torn-%s-%s", payload ? "payload" : "header",
             loaded_first ? "loaded" : "restart");
    if (!set_case_dir(root, case_name)) return 0;
    manifest_file(path);
    if (loaded_first) manifest_add(&a);
    else CHECK(write_bytes(path, expected_a, a_n, "wb"), "write complete prefix before restart");
    if (payload) {
        memcpy(torn, hdr_b, sizeof(hdr_b));
        memcpy(torn + sizeof(hdr_b), words_b, 2u * sizeof(words_b[0]));
        torn_n = sizeof(hdr_b) + 2u * sizeof(words_b[0]);
    } else {
        memcpy(torn, hdr_b, 7u);
        torn_n = 7u;
    }
    CHECK(write_bytes(path, torn, torn_n, "ab"), "append synthetic torn record tail");
    manifest_add(&c);
    memcpy(final, expected_a, a_n);
    memcpy(final + a_n, expected_c, c_n);
    final_n = a_n + c_n;
    CHECK(file_equals(path, final, final_n), payload ?
          "partial payload is removed before append, preserving the full prefix" :
          "partial header is removed before append, preserving the full prefix");
    CHECK(manifest_has(((uint64_t)c.sh << 32) | c.len), "record appended after a torn tail is recognized in memory");
    return 1;
}

static int run_invalid_tail(const char *root, int bad_length)
{
    uint32_t words[4] = { 0xA1u, 0xA2u, 0xA3u, 0xA4u };
    uint8_t corrupt[32], expected[64];
    uint32_t hdr[4] = { KG_MAN_MAGIC, 0x55555555u, 112u, 4u };
    char name[40], path[600];
    KGpuVs e = entry(0x66666666u, 144u, words, 4u);
    size_t n = record_bytes(expected, e.sh, e.len, words, e.nwords);
    snprintf(name, sizeof(name), bad_length ? "bad-record-length" : "bad-record-magic");
    if (!set_case_dir(root, name)) return 0;
    manifest_file(path);
    if (bad_length) hdr[3] = 3u; else hdr[0] ^= 0xFFFFFFFFu;
    memcpy(corrupt, hdr, sizeof(hdr));
    memset(corrupt + sizeof(hdr), 0xCC, sizeof(corrupt) - sizeof(hdr));
    CHECK(write_bytes(path, corrupt, sizeof(corrupt), "wb"), "write an invalid record header and tail");
    manifest_add(&e);
    CHECK(file_equals(path, expected, n), bad_length ?
          "invalid instruction count tail is discarded before append" :
          "invalid magic tail is discarded before append");
    return 1;
}

static int run_no_valid_prefix(const char *root)
{
    uint32_t words[4] = { 21u, 22u, 23u, 24u };
    uint8_t expected[64], torn[7] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u };
    char path[600];
    KGpuVs e = entry(0x77777777u, 160u, words, 4u);
    const size_t n = record_bytes(expected, e.sh, e.len, words, e.nwords);
    if (!set_case_dir(root, "no-valid-prefix")) return 0;
    manifest_file(path);
    CHECK(write_bytes(path, torn, sizeof(torn), "wb"), "write tail with no complete prefix");
    manifest_add(&e);
    CHECK(file_equals(path, expected, n), "a manifest with no complete prefix is rebuilt from the new record");
    return 1;
}

static int run_failed_append_retry(const char *root)
{
    uint32_t words[4] = { 31u, 32u, 33u, 34u };
    uint8_t expected[64];
    char good_dir[MAX_PATH], bad_dir[MAX_PATH], path[600];
    KGpuVs e = entry(0x88888888u, 192u, words, 4u);
    const size_t n = record_bytes(expected, e.sh, e.len, words, e.nwords);
    if (!set_case_dir(root, "retry-good")) return 0;
    snprintf(good_dir, sizeof(good_dir), "%s", s_cache_dir);
    snprintf(bad_dir, sizeof(bad_dir), "%s\\missing-parent\\nested", root);
    snprintf(s_cache_dir, sizeof(s_cache_dir), "%s", bad_dir);
    reset_state();
    manifest_add(&e);
    CHECK(!manifest_has(((uint64_t)e.sh << 32) | e.len), "failed file open does not mark the key written");
    snprintf(s_cache_dir, sizeof(s_cache_dir), "%s", good_dir);
    manifest_add(&e);
    manifest_file(path);
    CHECK(file_equals(path, expected, n), "failed append can be retried successfully");
    CHECK(manifest_has(((uint64_t)e.sh << 32) | e.len), "retry records the key only after durable append");
    return 1;
}

int main(int argc, char **argv)
{
    const char *root = argc > 1 ? argv[1] : ".";
    CreateDirectoryA(root, NULL);
    if (!run_empty_and_duplicate(root) || !run_duplicate_existing(root) ||
        !run_torn_tail(root, 0, 0) || !run_torn_tail(root, 1, 0) ||
        !run_torn_tail(root, 0, 1) || !run_torn_tail(root, 1, 1) ||
        !run_invalid_tail(root, 0) || !run_invalid_tail(root, 1) ||
        !run_no_valid_prefix(root) || !run_failed_append_retry(root)) return 2;
    printf("%u checks, %u failures\n", s_checks, s_failures);
    return s_failures ? 1 : 0;
}
