#pragma once
// Wi-Fi network management for the Zanop desktop.
//
// This header holds the whole manager (network list, connect/disconnect,
// saved networks, automatic reconnection, error reporting). It is header-only
// and uses no kernel headers, so it builds with the desktop shell and also
// under a normal host compiler for unit tests (tests/wifi/test_wifi.cpp).
//
// The manager talks to hardware through a `Backend`. Zanop has no 802.11
// driver yet, so the only backend shipped here is a *simulated* adapter
// (`simulated_backend()`); the UI says so on screen. A real driver plugs in
// by providing its own `Backend` and calling `Manager::init(&backend)`.
namespace wifi {

using size_t = decltype(sizeof(0));

constexpr size_t MAX_SSID = 32;      // 802.11 limit
constexpr size_t MIN_PASSPHRASE = 8; // WPA2-PSK passphrase limits
constexpr size_t MAX_PASSPHRASE = 63;
constexpr size_t MAX_NETWORKS = 12;  // scan results kept
constexpr size_t MAX_SAVED = 8;
constexpr int MIN_USABLE_SIGNAL = 15; // below this a connection attempt times out

enum class Security : unsigned char { Open, Wpa2 };

enum class Error : unsigned char {
    None,
    NoAdapter,       // no Wi-Fi adapter / backend
    NotFound,        // network is not in range
    PasswordLength,  // WPA2 passphrase must be 8..63 characters
    BadPassword,     // access point rejected the credentials
    WeakSignal,      // signal too weak, attempt timed out
    NotConnected,    // nothing to disconnect
};

struct Network {
    char ssid[MAX_SSID + 1];
    Security security;
    int signal; // 0..100 (percent)
};

struct Backend {
    const char* name;
    bool simulated;
    bool (*present)();
    size_t (*scan)(Network* out, size_t max);
    Error (*connect)(const char* ssid, const char* passphrase);
    void (*disconnect)();
};

namespace detail {
inline size_t len(const char* s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}
inline bool same(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
inline void copy(char* dst, size_t cap, const char* src) {
    size_t i = 0;
    for (; i + 1 < cap && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}
} // namespace detail

inline const char* error_text(Error e) {
    switch (e) {
        case Error::None: return "";
        case Error::NoAdapter: return "NO WI-FI ADAPTER FOUND";
        case Error::NotFound: return "NETWORK NOT IN RANGE";
        case Error::PasswordLength: return "PASSWORD MUST BE 8 TO 63 CHARACTERS";
        case Error::BadPassword: return "AUTHENTICATION FAILED - WRONG PASSWORD";
        case Error::WeakSignal: return "CONNECTION TIMED OUT - SIGNAL TOO WEAK";
        case Error::NotConnected: return "NOT CONNECTED TO ANY NETWORK";
    }
    return "";
}

struct SavedNetwork {
    char ssid[MAX_SSID + 1];
    char passphrase[MAX_PASSPHRASE + 1]; // empty for open networks
};

class Manager {
public:
    constexpr Manager() = default; // constant-initialised: no static-init guard needed
    void init(const Backend* backend) {
        backend_ = backend;
        count_ = 0;
        connected_ = false;
        manual_disconnect_ = false;
        last_error_ = Error::None;
        saved_count_ = 0;
        if (!adapter_present()) last_error_ = Error::NoAdapter;
    }

    const Backend* backend() const { return backend_; }
    bool adapter_present() const { return backend_ && backend_->present && backend_->present(); }

    // ---- scan results ----------------------------------------------------
    size_t network_count() const { return count_; }
    const Network& network(size_t i) const { return networks_[i]; }

    // Re-scans, strongest first. If the connected network vanished the link
    // is dropped; if we are not connected, a saved network in range is joined
    // (unless the user disconnected on purpose).
    void refresh() {
        if (!adapter_present()) {
            count_ = 0;
            connected_ = false;
            last_error_ = Error::NoAdapter;
            return;
        }
        Network found[MAX_NETWORKS];
        size_t n = backend_->scan(found, MAX_NETWORKS);
        if (n > MAX_NETWORKS) n = MAX_NETWORKS;
        for (size_t i = 1; i < n; ++i) { // insertion sort, strongest first
            Network key = found[i];
            size_t j = i;
            while (j > 0 && found[j - 1].signal < key.signal) { found[j] = found[j - 1]; --j; }
            found[j] = key;
        }
        count_ = n;
        for (size_t i = 0; i < n; ++i) networks_[i] = found[i];

        if (connected_) {
            const int s = signal_of(connected_ssid_);
            if (s < 0) { // network went away
                connected_ = false;
                if (backend_->disconnect) backend_->disconnect();
                last_error_ = Error::NotFound;
            }
        } else {
            auto_reconnect();
        }
    }

    // ---- connection ------------------------------------------------------
    bool connected() const { return connected_; }
    const char* connected_ssid() const { return connected_ ? connected_ssid_ : ""; }
    int connected_signal() const { return connected_ ? signal_of(connected_ssid_) : -1; }
    Error last_error() const { return last_error_; }

    // Joins `ssid`. For a saved network `passphrase` may be null and the saved
    // one is used. On success the credentials are remembered.
    Error connect(const char* ssid, const char* passphrase) {
        const Error e = try_connect(ssid, passphrase);
        if (e == Error::None) manual_disconnect_ = false; // a failed try keeps the user's disconnect
        last_error_ = e;
        return e;
    }

    Error disconnect() {
        if (!connected_) { last_error_ = Error::NotConnected; return last_error_; }
        if (backend_->disconnect) backend_->disconnect();
        connected_ = false;
        manual_disconnect_ = true; // do not auto-rejoin behind the user's back
        last_error_ = Error::None;
        return Error::None;
    }

    // ---- saved networks --------------------------------------------------
    size_t saved_count() const { return saved_count_; }
    const SavedNetwork& saved(size_t i) const { return saved_[i]; }
    bool is_saved(const char* ssid) const { return find_saved(ssid) >= 0; }

    // Forgets a network; if it is the connected one, also disconnects.
    bool forget(const char* ssid) {
        const int i = find_saved(ssid);
        if (i < 0) return false;
        for (size_t k = static_cast<size_t>(i); k + 1 < saved_count_; ++k) saved_[k] = saved_[k + 1];
        --saved_count_;
        if (connected_ && detail::same(connected_ssid_, ssid)) {
            if (backend_->disconnect) backend_->disconnect();
            connected_ = false;
            manual_disconnect_ = true;
        }
        return true;
    }

    // ---- persistence -----------------------------------------------------
    // Format, one record per line: "<ssid length>,<passphrase length>,<ssid><passphrase>\n"
    // (lengths make it safe for any characters in either field).
    // NOTE: the passphrase is stored in clear text; see docs/wifi.md.
    size_t serialize(char* out, size_t cap) const {
        size_t pos = 0;
        for (size_t i = 0; i < saved_count_; ++i) {
            const size_t sl = detail::len(saved_[i].ssid), pl = detail::len(saved_[i].passphrase);
            char head[16];
            size_t h = 0;
            h = put_num(head, h, sl);
            head[h++] = ',';
            h = put_num(head, h, pl);
            head[h++] = ',';
            if (pos + h + sl + pl + 1 >= cap) break;
            for (size_t k = 0; k < h; ++k) out[pos++] = head[k];
            for (size_t k = 0; k < sl; ++k) out[pos++] = saved_[i].ssid[k];
            for (size_t k = 0; k < pl; ++k) out[pos++] = saved_[i].passphrase[k];
            out[pos++] = '\n';
        }
        if (pos < cap) out[pos] = '\0';
        return pos;
    }

    // Replaces the saved list from serialize() output. Malformed records stop
    // the parse; records read so far are kept. Returns the number loaded.
    size_t load(const char* text, size_t size) {
        saved_count_ = 0;
        size_t pos = 0;
        while (pos < size && saved_count_ < MAX_SAVED) {
            size_t sl = 0, pl = 0;
            if (!take_num(text, size, pos, sl) || !take_num(text, size, pos, pl)) break;
            if (sl == 0 || sl > MAX_SSID || pl > MAX_PASSPHRASE) break;
            if (pos + sl + pl + 1 > size || text[pos + sl + pl] != '\n') break;
            SavedNetwork& s = saved_[saved_count_];
            for (size_t k = 0; k < sl; ++k) s.ssid[k] = text[pos + k];
            s.ssid[sl] = '\0';
            for (size_t k = 0; k < pl; ++k) s.passphrase[k] = text[pos + sl + k];
            s.passphrase[pl] = '\0';
            pos += sl + pl + 1;
            ++saved_count_;
        }
        return saved_count_;
    }

    // Set whenever the saved list changed since the last call.
    bool take_dirty() { const bool d = dirty_; dirty_ = false; return d; }

    // Joins the strongest saved network that is in range.
    bool auto_reconnect() {
        if (connected_ || manual_disconnect_ || !adapter_present()) return false;
        for (size_t i = 0; i < count_; ++i) { // networks_ is strongest-first
            if (find_saved(networks_[i].ssid) < 0) continue;
            if (try_connect(networks_[i].ssid, nullptr) == Error::None) {
                last_error_ = Error::None;
                return true;
            }
        }
        return false;
    }

private:
    int signal_of(const char* ssid) const {
        for (size_t i = 0; i < count_; ++i)
            if (detail::same(networks_[i].ssid, ssid)) return networks_[i].signal;
        return -1;
    }
    int find_saved(const char* ssid) const {
        for (size_t i = 0; i < saved_count_; ++i)
            if (detail::same(saved_[i].ssid, ssid)) return static_cast<int>(i);
        return -1;
    }
    static size_t put_num(char* out, size_t pos, size_t v) {
        char d[8];
        size_t n = 0;
        do { d[n++] = static_cast<char>('0' + v % 10); v /= 10; } while (v);
        while (n) out[pos++] = d[--n];
        return pos;
    }
    static bool take_num(const char* t, size_t size, size_t& pos, size_t& out) {
        size_t v = 0, digits = 0;
        while (pos < size && t[pos] >= '0' && t[pos] <= '9' && digits < 4) {
            v = v * 10 + static_cast<size_t>(t[pos] - '0');
            ++pos;
            ++digits;
        }
        if (digits == 0 || pos >= size || t[pos] != ',') return false;
        ++pos;
        out = v;
        return true;
    }

    Error try_connect(const char* ssid, const char* passphrase) {
        if (!adapter_present()) return Error::NoAdapter;
        const Network* net = nullptr;
        for (size_t i = 0; i < count_; ++i)
            if (detail::same(networks_[i].ssid, ssid)) { net = &networks_[i]; break; }
        if (!net) return Error::NotFound;

        const int saved_index = find_saved(ssid);
        if (!passphrase && saved_index >= 0) passphrase = saved_[saved_index].passphrase;
        if (net->security == Security::Wpa2) {
            const size_t pl = passphrase ? detail::len(passphrase) : 0;
            if (pl < MIN_PASSPHRASE || pl > MAX_PASSPHRASE) return Error::PasswordLength;
        } else {
            passphrase = "";
        }

        if (connected_ && backend_->disconnect) backend_->disconnect(); // leave the old network
        connected_ = false;
        const Error e = backend_->connect(ssid, passphrase);
        if (e != Error::None) return e;

        connected_ = true;
        detail::copy(connected_ssid_, sizeof(connected_ssid_), ssid);
        remember(ssid, passphrase);
        return Error::None;
    }

    void remember(const char* ssid, const char* passphrase) {
        int i = find_saved(ssid);
        if (i < 0) {
            if (saved_count_ == MAX_SAVED) { // drop the oldest to make room
                for (size_t k = 0; k + 1 < saved_count_; ++k) saved_[k] = saved_[k + 1];
                --saved_count_;
            }
            i = static_cast<int>(saved_count_++);
            detail::copy(saved_[i].ssid, sizeof(saved_[i].ssid), ssid);
            saved_[i].passphrase[0] = '\0';
            dirty_ = true;
        }
        if (!detail::same(saved_[i].passphrase, passphrase)) {
            detail::copy(saved_[i].passphrase, sizeof(saved_[i].passphrase), passphrase);
            dirty_ = true;
        }
    }

    const Backend* backend_ = nullptr;
    Network networks_[MAX_NETWORKS] = {};
    size_t count_ = 0;
    SavedNetwork saved_[MAX_SAVED] = {};
    size_t saved_count_ = 0;
    char connected_ssid_[MAX_SSID + 1] = {0};
    bool connected_ = false;
    bool manual_disconnect_ = false;
    bool dirty_ = false;
    Error last_error_ = Error::None;
};

// ---------------------------------------------------------------------------
// Simulated adapter: stands in for a driver that does not exist yet, so the
// manager and its UI can be exercised (QEMU has no Wi-Fi hardware). Signal
// strengths drift a little on every scan and one network comes and goes.
// Demo passphrases are in docs/wifi.md.
// ---------------------------------------------------------------------------
namespace sim {

struct Entry {
    const char* ssid;
    Security security;
    int base_signal;
    const char* passphrase;
};

inline const Entry* entries(size_t& n) {
    static const Entry table[] = {
        {"ZanopLab",     Security::Wpa2, 88, "zanop1234"},
        {"HomeNet-5G",   Security::Wpa2, 64, "homepass1"},
        {"CafeGuest",    Security::Open, 47, ""},
        {"Neighbor24",   Security::Wpa2, 14, "secret123"},
        {"PopUpHotspot", Security::Wpa2, 55, "popup5678"}, // only on every other scan
    };
    n = sizeof(table) / sizeof(table[0]);
    return table;
}

struct State {
    unsigned scans = 0;
    bool joined = false;
};
inline State g_state;
inline State& state() { return g_state; }

inline int signal_for(size_t index, unsigned scan) {
    size_t n;
    const Entry* t = entries(n);
    int s = t[index].base_signal + static_cast<int>((scan * 7 + index * 13) % 11) - 5;
    if (s < 1) s = 1;
    if (s > 100) s = 100;
    return s;
}

inline bool present() { return true; }

inline size_t scan(Network* out, size_t max) {
    State& st = state();
    ++st.scans;
    size_t n;
    const Entry* t = entries(n);
    size_t count = 0;
    for (size_t i = 0; i < n && count < max; ++i) {
        if (i == 4 && (st.scans % 2) == 0) continue;
        detail::copy(out[count].ssid, sizeof(out[count].ssid), t[i].ssid);
        out[count].security = t[i].security;
        out[count].signal = signal_for(i, st.scans);
        ++count;
    }
    return count;
}

inline Error connect(const char* ssid, const char* passphrase) {
    State& st = state();
    size_t n;
    const Entry* t = entries(n);
    for (size_t i = 0; i < n; ++i) {
        if (!detail::same(t[i].ssid, ssid)) continue;
        if (signal_for(i, st.scans) < MIN_USABLE_SIGNAL) return Error::WeakSignal;
        if (t[i].security == Security::Wpa2 && !detail::same(t[i].passphrase, passphrase))
            return Error::BadPassword;
        st.joined = true;
        return Error::None;
    }
    return Error::NotFound;
}

inline void disconnect() { state().joined = false; }

} // namespace sim

inline const Backend& simulated_backend() {
    static const Backend b = {"SIMULATED ADAPTER", true, sim::present, sim::scan,
                              sim::connect, sim::disconnect};
    return b;
}

// Constant-initialised global; the freestanding kernel has no __cxa_guard_*.
inline Manager g_manager;
inline Manager& manager() { return g_manager; }

} // namespace wifi
