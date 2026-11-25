// Host unit tests for shell/src/wifi.hpp.
//   g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -Ishell/src tests/wifi/test_wifi.cpp -o /tmp/test_wifi && /tmp/test_wifi
#include "wifi.hpp"
#include <cstdio>
#include <cstring>

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; \
    std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

using namespace wifi;

static Manager fresh() {
    Manager m;
    m.init(&simulated_backend());
    m.refresh();
    return m;
}

// A backend with no adapter.
static bool absent() { return false; }
static size_t no_scan(Network*, size_t) { return 0; }
static Error no_connect(const char*, const char*) { return Error::NoAdapter; }
static void no_disc() {}

static int index_of(const Manager& m, const char* ssid) {
    for (size_t i = 0; i < m.network_count(); ++i)
        if (!std::strcmp(m.network(i).ssid, ssid)) return static_cast<int>(i);
    return -1;
}

static void test_scan() {
    Manager m = fresh();
    CHECK(m.network_count() >= 4);
    for (size_t i = 1; i < m.network_count(); ++i)
        CHECK(m.network(i - 1).signal >= m.network(i).signal); // strongest first
    for (size_t i = 0; i < m.network_count(); ++i)
        CHECK(m.network(i).signal >= 1 && m.network(i).signal <= 100);
    // the pop-up hotspot appears only on alternate scans
    const bool first = index_of(m, "PopUpHotspot") >= 0;
    m.refresh();
    CHECK((index_of(m, "PopUpHotspot") >= 0) != first);
}

static void test_connect_errors() {
    Manager m = fresh();
    CHECK(m.connect("Nope", "whatever1") == Error::NotFound);
    CHECK(!m.connected());
    CHECK(m.connect("ZanopLab", "short") == Error::PasswordLength);
    CHECK(m.connect("ZanopLab", nullptr) == Error::PasswordLength);
    char longpw[80];
    std::memset(longpw, 'a', 70); longpw[70] = 0;
    CHECK(m.connect("ZanopLab", longpw) == Error::PasswordLength);
    CHECK(m.connect("ZanopLab", "wrongpass1") == Error::BadPassword);
    CHECK(!m.connected());
    CHECK(m.last_error() == Error::BadPassword);
    CHECK(m.saved_count() == 0); // failed attempts are never saved
}

static void test_connect_ok() {
    Manager m = fresh();
    CHECK(m.connect("ZanopLab", "zanop1234") == Error::None);
    CHECK(m.connected());
    CHECK(!std::strcmp(m.connected_ssid(), "ZanopLab"));
    CHECK(m.connected_signal() > 50);
    CHECK(m.last_error() == Error::None);
    CHECK(m.saved_count() == 1);
    CHECK(m.take_dirty());
    CHECK(!m.take_dirty());
    // open network ignores any passphrase and is saved with none
    CHECK(m.connect("CafeGuest", nullptr) == Error::None);
    CHECK(!std::strcmp(m.connected_ssid(), "CafeGuest")); // switched networks
    CHECK(m.saved_count() == 2);
    CHECK(m.saved(1).passphrase[0] == '\0');
}

static void test_weak_signal() {
    Manager m = fresh();
    bool saw_weak = false;
    for (int i = 0; i < 12 && !saw_weak; ++i) {
        const int idx = index_of(m, "Neighbor24");
        if (idx >= 0 && m.network(static_cast<size_t>(idx)).signal < MIN_USABLE_SIGNAL) {
            CHECK(m.connect("Neighbor24", "secret123") == Error::WeakSignal);
            CHECK(!m.connected());
            saw_weak = true;
        }
        m.refresh();
    }
    CHECK(saw_weak);
}

static void test_disconnect_forget() {
    Manager m = fresh();
    CHECK(m.disconnect() == Error::NotConnected);
    m.connect("ZanopLab", "zanop1234");
    CHECK(m.disconnect() == Error::None);
    CHECK(!m.connected());
    CHECK(m.is_saved("ZanopLab")); // disconnect keeps credentials
    // a manual disconnect must not be undone by the next scan
    for (int i = 0; i < 4; ++i) m.refresh();
    CHECK(!m.connected());
    // a failed manual attempt must not re-enable automatic joining either
    CHECK(m.connect("HomeNet-5G", "wrongpass1") == Error::BadPassword);
    for (int i = 0; i < 4; ++i) m.refresh();
    CHECK(!m.connected());
    // explicit connect with saved credentials needs no passphrase
    CHECK(m.connect("ZanopLab", nullptr) == Error::None);
    CHECK(m.forget("ZanopLab"));
    CHECK(!m.connected()); // forgetting the active network drops it
    CHECK(!m.is_saved("ZanopLab"));
    CHECK(!m.forget("ZanopLab"));
    CHECK(m.connect("ZanopLab", nullptr) == Error::PasswordLength); // credentials gone
}

static void test_auto_reconnect() {
    Manager a = fresh();
    a.connect("HomeNet-5G", "homepass1");
    a.connect("ZanopLab", "zanop1234");
    char buf[512];
    const size_t n = a.serialize(buf, sizeof(buf));

    // "reboot": new manager loads saved list and reconnects to the strongest.
    Manager b;
    b.init(&simulated_backend());
    CHECK(b.load(buf, n) == 2);
    CHECK(!b.connected());
    b.refresh();
    CHECK(b.connected());
    CHECK(!std::strcmp(b.connected_ssid(), "ZanopLab"));

    // after forgetting everything there is nothing to rejoin
    Manager c;
    c.init(&simulated_backend());
    c.refresh();
    CHECK(!c.connected());
}

static void test_persistence() {
    Manager m = fresh();
    m.connect("ZanopLab", "zanop1234");
    m.connect("CafeGuest", nullptr);
    char buf[512];
    const size_t n = m.serialize(buf, sizeof(buf));
    CHECK(n > 0 && buf[n] == '\0');
    Manager r;
    r.init(&simulated_backend());
    CHECK(r.load(buf, n) == 2);
    CHECK(!std::strcmp(r.saved(0).ssid, "ZanopLab"));
    CHECK(!std::strcmp(r.saved(0).passphrase, "zanop1234"));
    CHECK(!std::strcmp(r.saved(1).ssid, "CafeGuest"));
    CHECK(r.saved(1).passphrase[0] == '\0');

    // awkward characters round-trip: commas, newlines, spaces
    Manager w;
    w.init(&simulated_backend());
    char ok[64];
    const int l = std::snprintf(ok, sizeof(ok), "7,10,my,net pass\nword1\n");
    CHECK(w.load(ok, static_cast<size_t>(l)) == 1);
    CHECK(!std::strcmp(w.saved(0).ssid, "my,net "));
    CHECK(!std::strcmp(w.saved(0).passphrase, "pass\nword1"));

    // truncated serialize buffer never overflows and yields whole records
    char small[20];
    const size_t sn = m.serialize(small, sizeof(small));
    CHECK(sn < sizeof(small));
}

static void test_malformed_load() {
    Manager m;
    m.init(&simulated_backend());
    const char* bad[] = {"", "x", "5,3,", "5,3,abcde", "0,0,\n", "33,0,", "5,64,abcde",
                         "5,3,abcdexyz", "5,3,abcdexyzQ", "99999,1,a\n", "-1,2,ab\n", ",,\n"};
    for (const char* b : bad) {
        CHECK(m.load(b, std::strlen(b)) == 0);
        CHECK(m.saved_count() == 0);
    }
    // good record followed by garbage keeps the good one
    const char* mixed = "3,8,abcpassword\ngarbage";
    CHECK(m.load(mixed, std::strlen(mixed)) == 1);
    // more than MAX_SAVED records are capped
    char many[1024];
    size_t pos = 0;
    for (int i = 0; i < 12; ++i) pos += static_cast<size_t>(std::snprintf(many + pos, 40, "2,0,n%d\n", i % 10));
    CHECK(m.load(many, pos) == MAX_SAVED);
}

static void test_saved_cap() {
    // a full list evicts the oldest entry when a new network is joined
    Manager m = fresh();
    char rec[1024];
    size_t pos = 0;
    for (size_t k = 0; k < MAX_SAVED; ++k)
        pos += static_cast<size_t>(std::snprintf(rec + pos, 40, "2,0,s%zu\n", k));
    CHECK(m.load(rec, pos) == MAX_SAVED);
    CHECK(m.connect("CafeGuest", nullptr) == Error::None);
    CHECK(m.saved_count() == MAX_SAVED);
    CHECK(m.is_saved("CafeGuest"));
    CHECK(!m.is_saved("s0"));
    CHECK(m.is_saved("s1"));
}

static void test_no_adapter() {
    static const Backend none = {"none", false, absent, no_scan, no_connect, no_disc};
    Manager m;
    m.init(&none);
    CHECK(!m.adapter_present());
    CHECK(m.last_error() == Error::NoAdapter);
    m.refresh();
    CHECK(m.network_count() == 0);
    CHECK(m.connect("x", "y") == Error::NoAdapter);
    Manager n;
    n.init(nullptr); // no backend at all must not crash
    n.refresh();
    CHECK(n.connect("x", "y") == Error::NoAdapter);
    CHECK(n.network_count() == 0);
}

static void test_vanishing_network() {
    // connect to the hotspot that only appears on alternate scans
    Manager m = fresh();
    if (index_of(m, "PopUpHotspot") < 0) m.refresh();
    CHECK(index_of(m, "PopUpHotspot") >= 0);
    CHECK(m.connect("PopUpHotspot", "popup5678") == Error::None);
    m.refresh(); // it vanishes
    CHECK(index_of(m, "PopUpHotspot") < 0);
    CHECK(!m.connected());
    CHECK(m.last_error() == Error::NotFound);
    m.refresh(); // it is back: saved network is rejoined automatically
    CHECK(m.connected());
}

static void test_error_text() {
    for (int e = 0; e <= 6; ++e) {
        const char* t = error_text(static_cast<Error>(e));
        CHECK(t != nullptr);
        if (e != 0) CHECK(t[0] != '\0');
    }
}

int main() {
    test_scan();
    test_connect_errors();
    test_connect_ok();
    test_weak_signal();
    test_disconnect_forget();
    test_auto_reconnect();
    test_persistence();
    test_malformed_load();
    test_saved_cap();
    test_no_adapter();
    test_vanishing_network();
    test_error_text();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
