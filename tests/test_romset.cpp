// S16 test of src/board/romset.cpp. Usage: test_romset PATH_TO_pacman.zip [prog.bin]
// Build (native):  see tools/ ... or the command in docs/evidence/S16_test.txt (g++ + miniz C files, flags -DMINIZ_NO_STDIO -DMINIZ_NO_TIME).
// The test never writes anything to disk: all damaged / reduced test archives are built in memory.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/board/romset.h"
#include "../src/third_party/miniz/miniz.h"

static int failures = 0, checks = 0;
static void expect(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what);
    }
}
static bool contains(const std::string &s, const char *t) { return s.find(t) != std::string::npos; }

static std::vector<uint8_t> read_file(const char *p) {
    std::vector<uint8_t> v;
    FILE *f = std::fopen(p, "rb");
    if (!f) return v;
    uint8_t b[65536];
    size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + n);
    std::fclose(f);
    return v;
}
struct Entry {
    std::string name;
    std::vector<uint8_t> data;
};
static std::vector<Entry> unpack(const std::vector<uint8_t> &zip) {
    std::vector<Entry> out;
    mz_zip_archive z;
    std::memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_mem(&z, zip.data(), zip.size(), 0)) return out;
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&z); ++i) {
        mz_zip_archive_file_stat st;
        mz_zip_reader_file_stat(&z, i, &st);
        Entry e;
        e.name = st.m_filename;
        e.data.resize((size_t)st.m_uncomp_size);
        mz_zip_reader_extract_to_mem(&z, i, e.data.data(), e.data.size(), 0);
        out.push_back(std::move(e));
    }
    mz_zip_reader_end(&z);
    return out;
}
static std::vector<uint8_t> pack(const std::vector<Entry> &es) {
    mz_zip_archive z;
    std::memset(&z, 0, sizeof z);
    mz_zip_writer_init_heap(&z, 0, 0);
    for (auto &e : es) mz_zip_writer_add_mem(&z, e.name.c_str(), e.data.data(), e.data.size(), MZ_BEST_COMPRESSION);
    void *p = nullptr;
    size_t n = 0;
    mz_zip_writer_finalize_heap_archive(&z, &p, &n);
    std::vector<uint8_t> v((uint8_t *)p, (uint8_t *)p + n);
    mz_zip_writer_end(&z);
    mz_free(p);
    return v;
}
static bool load(const std::vector<uint8_t> &z, RomSet &r, std::string &err) { return load_romset_from_memory(z.data(), z.size(), r, err); }
static void show(const char *label, bool ok, const std::string &err) {
    std::printf("[%s] %s\n%s", label, ok ? "loaded" : "REFUSED", err.empty() ? "" : ("    " + err).c_str());
    if (!err.empty() && err.back() != '\n') std::printf("\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_romset pacman.zip [prog.bin]\n");
        return 2;
    }
    std::vector<uint8_t> real = read_file(argv[1]);
    RomSet r;
    std::string err;

    std::printf("== 1. real zip (from file)\n");
    bool ok = load_romset_from_file(argv[1], r, err);
    show("real file", ok, err);
    expect(ok, "real zip loads");
    expect(r.program.size() == 16384 && r.tiles.size() == 4096 && r.sprites.size() == 4096 && r.colours.size() == 32 && r.palettes.size() == 256 &&
               r.sound1.size() == 256 && r.sound3.size() == 256,
           "all parts have the right sizes");
    if (argc > 2) {
        auto p = read_file(argv[2]);
        expect(p == r.program, "program equals build/rom/prog.bin (the image the prototype and disassembly use)");
    }

    std::vector<Entry> es = unpack(real);
    expect(es.size() >= 10, "unpacked the real zip for building test archives");

    std::printf("== 2. corrupted copy (one byte changed inside pacman.6h)\n");
    {
        auto c = es;
        for (auto &e : c)
            if (e.name == "pacman.6h") e.data[1234] ^= 0x01;
        ok = load(pack(c), r, err);
        show("corrupted", ok, err);
        expect(!ok && contains(err, "pacman.6h") && contains(err, "wrong checksum") && contains(err, "bcdd1beb"),
               "names the file, says wrong checksum, gives the expected CRC");
        expect(r.program.empty(), "nothing is returned for a refused set");
    }

    std::printf("== 3. missing-file copy (82s126.3m removed)\n");
    {
        std::vector<Entry> c;
        for (auto &e : es)
            if (e.name != "82s126.3m") c.push_back(e);
        ok = load(pack(c), r, err);
        show("missing file", ok, err);
        expect(!ok && contains(err, "82s126.3m") && contains(err, "missing"), "names the missing file");
    }

    std::printf("== 4. wrong size (pacman.5e one byte short)\n");
    {
        auto c = es;
        for (auto &e : c)
            if (e.name == "pacman.5e") e.data.pop_back();
        ok = load(pack(c), r, err);
        show("wrong size", ok, err);
        expect(!ok && contains(err, "pacman.5e") && contains(err, "wrong size") && contains(err, "4095") && contains(err, "4096"), "says wrong size with both numbers");
    }

    std::printf("== 5. not a zip\n");
    {
        std::vector<uint8_t> t;
        const char *s = "this is not a zip file";
        t.assign(s, s + std::strlen(s));
        ok = load(t, r, err);
        show("text file", ok, err);
        expect(!ok && contains(err, "not a zip"), "not-a-zip message");
        ok = load_romset_from_memory(nullptr, 0, r, err);
        expect(!ok && contains(err, "not a zip"), "empty input refused");
        std::vector<uint8_t> cut(real.begin(), real.begin() + real.size() / 2);
        ok = load(cut, r, err);
        show("zip cut in half", ok, err);
        expect(!ok && !err.empty(), "truncated zip refused with a message");
    }

    std::printf("== 6. damaged zip data (raw byte flipped inside the zip, not rebuilt)\n");
    {
        RomSet good;
        std::string e0;
        load_romset_from_memory(real.data(), real.size(), good, e0);
        int refused = 0, same = 0, total = 0;
        bool bad = false;
        for (size_t pos = 0; pos < real.size(); pos += 3) { // flip one byte at a time across the whole zip
            auto c = real;
            c[pos] ^= 0x55;
            ++total;
            ok = load(c, r, err);
            if (!ok) {
                ++refused;
                if (err.empty()) {
                    bad = true;
                    break;
                }
            } else if (!(r.program == good.program && r.tiles == good.tiles && r.sprites == good.sprites && r.colours == good.colours && r.palettes == good.palettes &&
                         r.sound1 == good.sound1 && r.sound3 == good.sound3)) {
                bad = true;
                break;
            } else ++same;
        }
        std::printf(
            "    %d byte flips: %d refused with a message, %d loaded with data identical to the real set (flip hit an unused zip field), 0 loaded with different data\n",
            total, refused, same);
        expect(!bad, "a damaged zip is never accepted with different data and every refusal has a message");
        expect(refused > 0, "some raw damage is detected");
    }

    std::printf("== 7. extra files ignored\n");
    {
        auto c = es;
        c.push_back({"readme.txt", {'h', 'i'}});
        c.push_back({"puckman.6e", std::vector<uint8_t>(4096, 0x55)});
        ok = load(pack(c), r, err);
        show("extra files", ok, err);
        expect(ok && r.program.size() == 16384, "set with extra files loads");
    }

    std::printf("== 8. file that does not exist\n");
    {
        ok = load_romset_from_file("no/such/pacman.zip", r, err);
        show("no such file", ok, err);
        expect(!ok && contains(err, "Cannot open"), "cannot-open message");
    }

    std::printf("== 9. several problems at once are all reported\n");
    {
        std::vector<Entry> c;
        for (auto &e : es)
            if (e.name != "pacman.6e") c.push_back(e);
        for (auto &e : c)
            if (e.name == "82s123.7f") e.data[0] ^= 1;
        ok = load(pack(c), r, err);
        show("two problems", ok, err);
        expect(!ok && contains(err, "pacman.6e") && contains(err, "82s123.7f"), "both problems listed");
    }

    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS test_romset", checks, failures);
    return failures ? 1 : 0;
}
