// OSTLERTEKKERS HEN - tests for games, cheats, GoldHEN info, backup, fs utils.
#include <string>

#include "backup/backup.h"
#include "check.h"
#include "cheats/cheat_db.h"
#include "core/sfo.h"
#include "core/sha256.h"
#include "files/fs_util.h"
#include "games/game_library.h"
#include "goldhen/goldhen_info.h"
#include "platform/mem_fs.h"

using namespace henu;

namespace {
void addGame(MemFileSystem& fs, const std::string& id, const std::string& name, const std::string& cid,
             const std::string& appVer, const std::string& ver) {
    fs.dirs.insert("/user/app/" + id);
    fs.dirs.insert("/system_data/priv/appmeta/" + id);
    Sfo s;
    s.strings["TITLE"] = name;
    s.strings["CONTENT_ID"] = cid;
    s.strings["CATEGORY"] = "gd";
    s.strings["APP_VER"] = appVer;
    s.strings["VERSION"] = ver;
    s.ints["APP_TYPE"] = 1;
    fs.files["/system_data/priv/appmeta/" + id + "/param.sfo"] = writeSfo(s);
}

const char* kJson = R"({"name":"Demo Racer","id":"CUSA99901","version":"01.02","process":"eboot.bin",
 "mods":[{"name":"Infinite Money","type":"checkbox","memory":[]},{"name":"Max Speed","type":"button","memory":[]}],
 "credits":["Tester"]})";
const char* kShn = R"(<?xml version="1.0" encoding="utf-8"?>
<Trainer Game="Demo Quest &amp; Friends" Moder="Someone" Cusa="CUSA99902" Version="01.00" Process="eboot.bin">
  <Cheat Text="Infinite Health"><Cheatline><Offset>1</Offset></Cheatline></Cheat>
  <Cheat Text="Unlimited Ammo" Description="all weapons"></Cheat>
</Trainer>)";

std::string utf16le(const std::string& ascii) {
    std::string out = "\xFF\xFE";
    for (char c : ascii) { out += c; out += '\0'; }
    return out;
}
}  // namespace

static void testSfoAndSha() {
    Sfo s;
    s.strings["TITLE"] = "Hällo";
    s.strings["APP_VER"] = "01.05";
    s.ints["APP_TYPE"] = 1;
    auto back = parseSfo(writeSfo(s));
    CHECK(back.isOk() && back.value().str("TITLE") == "Hällo" && back.value().ints.at("APP_TYPE") == 1);
    CHECK(!parseSfo("garbage").isOk());
    std::string trunc = writeSfo(s);
    trunc.resize(30);
    CHECK(!parseSfo(trunc).isOk());

    // FIPS 180-4 test vectors
    CHECK(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(sha256Hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

static void testGames() {
    CHECK(higherVersion("01.00", "01.05") == "01.05");
    CHECK(higherVersion("01.10", "01.09") == "01.10");
    CHECK(higherVersion("x", "01.02") == "01.02");
    CHECK(higherVersion("", "") == "");

    MemFileSystem fs;
    fs.dirs.insert("/user/app");
    addGame(fs, "CUSA99901", "Demo Racer", "EP9999-CUSA99901_00-DEMORACER0000000", "01.00", "01.02");
    addGame(fs, "CUSA99902", "Alpha Quest", "UP9999-CUSA99902_00-ALPHAQUEST000000", "01.00", "01.00");
    addGame(fs, "HENU00001", "OSTLERTEKKERS HEN", "IV0000-HENU00001_00-PS4HENULTIMATE00", "01.00", "01.00");
    fs.dirs.insert("/user/app/CUSA99903");   // no metadata
    fs.dirs.insert("/user/app/notatitle");
    GameLibrary lib;
    CHECK(lib.scan(fs).isOk());
    CHECK(lib.all().size() == 4);
    CHECK(lib.gameCount() == 3 && lib.homebrewCount() == 1);
    CHECK(lib.all().front().name == "Alpha Quest");   // sorted by name
    const GameInfo* g = lib.find("CUSA99901");
    CHECK(g && g->version == "01.02" && g->region == Region::EU && !g->homebrew);
    const GameInfo* h = lib.find("HENU00001");
    CHECK(h && h->homebrew);
    const GameInfo* n = lib.find("CUSA99903");
    CHECK(n && !n->metaFound && n->name == "CUSA99903" && n->version.empty());

    MemFileSystem empty;
    GameLibrary none;
    CHECK(!none.scan(empty).isOk() && none.all().empty());
}

static void testCheats() {
    auto j = parseCheatFile("CUSA99901_01.02.json", kJson);
    CHECK(j.isOk() && j.value().readable && j.value().cheats.size() == 2);
    CHECK(j.value().gameName == "Demo Racer" && j.value().process == "eboot.bin" && j.value().credits.size() == 1);
    CHECK(j.value().cheats[0].category == CheatCategory::Money && j.value().cheats[1].type == "button");
    CHECK(j.value().note.empty());
    CHECK(parseCheatFile("CUSA99901_01.00.json", kJson).value().note == "version-mismatch");
    CHECK(!parseCheatFile("CUSA99901_01.02.json", "{broken").isOk());
    CHECK(!parseCheatFile("CUSA99901_01.02.json", "{\"name\":\"x\"}").isOk());   // no mods

    auto s = parseCheatFile("CUSA99902_01.00.shn", kShn);
    CHECK(s.isOk() && s.value().cheats.size() == 2 && s.value().gameName == "Demo Quest & Friends");
    CHECK(s.value().cheats[1].description == "all weapons" && s.value().cheats[0].category == CheatCategory::Health);
    auto s16 = parseCheatFile("CUSA99902_01.00.shn", utf16le(kShn));
    CHECK(s16.isOk() && s16.value().cheats.size() == 2);
    CHECK(!parseCheatFile("CUSA99902_01.00.shn", "<xml/>").isOk());

    auto m = parseCheatFile("CUSA99902_01.00_2.mc4", "");
    CHECK(m.isOk() && !m.value().readable && m.value().id.variant == "2");

    MemFileSystem fs;
    const std::string root = "/data/GoldHEN/cheats";
    fs.dirs.insert(root + "/json");
    fs.dirs.insert(root + "/shn");
    fs.dirs.insert(root + "/mc4");
    fs.files[root + "/json/CUSA99901_01.02.json"] = kJson;
    fs.files[root + "/json/CUSA99901_01.00.json"] = kJson;
    fs.files[root + "/shn/CUSA99902_01.00.shn"] = kShn;
    fs.files[root + "/mc4/CUSA99903_01.00.mc4"] = "encrypted";
    fs.files[root + "/json/readme.txt"] = "ignored";
    fs.files[root + "/json/CUSA1_bad.json"] = "{}";       // looks like a cheat file, invalid name
    fs.files[root + "/json/CUSA99901_01.03.json"] = "{";  // broken content
    CheatDatabase db;
    db.scan(fs, root);
    CHECK(db.files().size() == 4);
    CHECK(db.count(CheatFormat::Json) == 2 && db.count(CheatFormat::Shn) == 1 && db.count(CheatFormat::Mc4) == 1);
    CHECK(db.errors().size() == 2);
    CHECK(db.totalCheats() == 6);
    CHECK(db.forTitle("CUSA99901").size() == 2);

    MemFileSystem gfs;
    gfs.dirs.insert("/user/app");
    addGame(gfs, "CUSA99901", "Demo Racer", "EP9999-CUSA99901_00-X", "01.00", "01.02");
    GameLibrary lib;
    lib.scan(gfs);
    int ok = 0, ver = 0, missing = 0;
    for (const auto& f : db.files()) {
        Compatibility c = compatibilityFor(f, lib);
        ok += c == Compatibility::Compatible;
        ver += c == Compatibility::VersionMismatch;
        missing += c == Compatibility::GameNotInstalled;
    }
    CHECK(ok == 1 && ver == 1 && missing == 2);

    const CheatFile& first = db.files().front();
    CHECK(matchesQuery(first, lib.find("CUSA99901"), "demo"));
    CHECK(matchesQuery(first, lib.find("CUSA99901"), "cusa999"));
    CHECK(matchesQuery(first, lib.find("CUSA99901"), "eu"));
    CHECK(!matchesQuery(first, lib.find("CUSA99901"), "jp"));
    CHECK(matchesQuery(first, nullptr, ""));
}

static void testGoldhen() {
    auto p = parsePluginsIni("; comment\n[default]\n/data/GoldHEN/plugins/game_patch.prx\n\n[CUSA00001]\r\n/data/GoldHEN/plugins/afr.prx\n");
    CHECK(p.size() == 2 && p[0].section == "default" && p[1].section == "CUSA00001" && p[1].path == "/data/GoldHEN/plugins/afr.prx");

    MemFileSystem fs;
    CHECK(!detectGoldhen(fs).folderFound);
    fs.dirs.insert("/data/GoldHEN");
    fs.dirs.insert("/data/GoldHEN/cheats/json");
    fs.files["/data/GoldHEN/cheats/json/CUSA99901_01.02.json"] = kJson;
    fs.files["/data/GoldHEN/plugins.ini"] = "[default]\n/data/GoldHEN/plugins/game_patch.prx\n";
    fs.files["/data/GoldHEN/plugins/game_patch.prx"] = "x";
    fs.files["/data/GoldHEN/config.ini"] = "x";
    GoldhenStatus s = detectGoldhen(fs);
    CHECK(s.folderFound && s.configFound && s.pluginsIniFound && s.cheatsJson == 1 && s.plugins.size() == 1 && s.plugins[0].fileExists);

    CHECK(minimumGoldhenFor("13.52") == "v2.4b18.11");
    CHECK(minimumGoldhenFor("13.50") == "v2.4b18.12");
    CHECK(minimumGoldhenFor("11.00") == "v2.4b18");
    CHECK(minimumGoldhenFor("14.00").empty());
}

static void testFsUtilAndBackup() {
    CHECK(joinPath("/a", "b") == "/a/b" && joinPath("/", "b") == "/b" && joinPath("/a/", "b") == "/a/b");
    CHECK(parentPath("/a/b") == "/a" && parentPath("/a") == "/" && baseName("/a/b.txt") == "b.txt");
    CHECK(formatBytes(512) == "512 B" && formatBytes(2048) == "2.0 KB");

    MemFileSystem fs;
    for (const char* d : {"/", "/data", "/data/OSTLERTEKKERS_HEN", "/data/GoldHEN", "/data/GoldHEN/cheats",
                          "/data/GoldHEN/cheats/json", "/mnt", "/mnt/usb0", "/system"})
        fs.dirs.insert(d);
    PathGuard guard("/data/OSTLERTEKKERS_HEN");
    fs.files["/data/OSTLERTEKKERS_HEN/settings.json"] = "{\"schemaVersion\":1}";
    fs.files["/data/OSTLERTEKKERS_HEN/favorites.json"] = "{}";
    fs.files["/data/GoldHEN/cheats/json/CUSA99901_01.02.json"] = kJson;
    fs.files["/system/secret"] = "x";

    CHECK(firstUsbRoot(fs) == "/mnt/usb0");
    CHECK(makeDirs(fs, guard, "/mnt/usb0/OSTLERTEKKERS_HEN/export").isOk());
    CHECK(fs.dirs.count("/mnt/usb0/OSTLERTEKKERS_HEN/export") == 1);
    CHECK(!makeDirs(fs, guard, "/system/new").isOk());
    CHECK(guardedCopy(fs, guard, "/system/secret", "/mnt/usb0/OSTLERTEKKERS_HEN/export/secret").isOk());   // reading is fine
    CHECK(!guardedCopy(fs, guard, "/mnt/usb0/OSTLERTEKKERS_HEN/export/secret", "/system/secret2").isOk());
    CHECK(!guardedRemoveTree(fs, guard, "/system").isOk());
    CHECK(fs.files.count("/system/secret") == 1);
    CHECK(!guardedRemoveTree(fs, guard, "/data/GoldHEN/config.ini").isOk());   // GoldHEN outside cheat folders
    // missing writable root may be created
    CHECK(makeDirs(fs, guard, "/data/GoldHEN/cheats/shn").isOk());

    BackupService b(fs, guard, "/data/OSTLERTEKKERS_HEN", "/data/GoldHEN/cheats");
    auto r1 = b.create("/data/OSTLERTEKKERS_HEN/backups", false, "2026-01-01_10-00-00");
    CHECK(r1.isOk() && r1.value().files == 2 && !r1.value().includesCheats);
    auto r2 = b.create("/mnt/usb0/OSTLERTEKKERS_HEN/backups", true, "2026-01-02_10-00-00");
    CHECK(r2.isOk() && r2.value().files == 3);
    CHECK(!b.create("/data/OSTLERTEKKERS_HEN/backups", false, "2026-01-01_10-00-00").isOk());   // exists
    CHECK(!b.create("/system/backups", false, "x").isOk());                                   // protected target
    auto list = b.list("/data/OSTLERTEKKERS_HEN/backups");
    CHECK(list.size() == 1 && list[0].manifestOk && list[0].files == 2);
    CHECK(b.verify(r2.value().dir).empty());

    // change data, restore, data comes back; a safety backup is made
    fs.files["/data/OSTLERTEKKERS_HEN/settings.json"] = "{\"changed\":true}";
    fs.files.erase("/data/GoldHEN/cheats/json/CUSA99901_01.02.json");
    CHECK(b.restore(r2.value().dir, "/data/OSTLERTEKKERS_HEN/backups/safety", "s1").isOk());
    CHECK(fs.files["/data/OSTLERTEKKERS_HEN/settings.json"] == "{\"schemaVersion\":1}");
    CHECK(fs.files.count("/data/GoldHEN/cheats/json/CUSA99901_01.02.json") == 1);
    CHECK(b.list("/data/OSTLERTEKKERS_HEN/backups/safety").size() == 1);

    // tampered file -> verify fails and restore is refused
    fs.files[r1.value().dir + "/files/data/OSTLERTEKKERS_HEN/settings.json"] = "tampered";
    CHECK(b.verify(r1.value().dir).size() == 1);
    CHECK(!b.restore(r1.value().dir, "/data/OSTLERTEKKERS_HEN/backups/safety", "s2").isOk());

    CHECK(b.remove(r1.value().dir).isOk());
    CHECK(b.list("/data/OSTLERTEKKERS_HEN/backups").empty() || b.list("/data/OSTLERTEKKERS_HEN/backups").size() == 1);
    CHECK(timestampNow().size() == 19);
}

void runModuleTests() {
    testSfoAndSha();
    testGames();
    testCheats();
    testGoldhen();
    testFsUtilAndBackup();
}
