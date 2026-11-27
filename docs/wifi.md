# Wi-Fi network management

Wi-Fi controls live in **Settings -> 3 Wi-Fi**. The top bar shows the current
connection (`WIFI <network> <signal>` or `WIFI NOT CONNECTED`).

## What works

| Requirement (issue #12)              | How |
|--------------------------------------|-----|
| List available networks              | Scan results, strongest first, with security (OPEN / WPA2) and signal bars |
| Select a network                     | Keys `1`-`8` |
| Password-protected networks          | Password prompt (masked, 8-63 characters, WPA2-PSK rules) |
| Disconnect                           | `D` (a manual disconnect is not undone by the next scan) |
| Connection status and signal         | Banner on the Wi-Fi screen, Settings line, top bar |
| Save credentials, auto-reconnect     | Saved on a successful connection; at boot and on refresh the strongest saved network in range is joined |
| Forget saved networks                | `F`, then the number of the network |
| Feedback for failures                | No adapter, not in range, wrong password, password length, weak signal |
| Refresh                              | `R` |

## Important: the adapter is simulated

Zanop has no 802.11 driver (there is no Wi-Fi hardware support, and QEMU has
no Wi-Fi device). The manager therefore talks to a `wifi::Backend`, and the
only backend included is `wifi::simulated_backend()`, which the screen labels
`SIMULATED ADAPTER`. It offers five fake networks whose signal drifts on every
scan (`PopUpHotspot` appears on every other scan, `Neighbor24` is usually too
weak to join):

| SSID | Security | Passphrase |
|------|----------|------------|
| ZanopLab | WPA2 | `zanop1234` |
| HomeNet-5G | WPA2 | `homepass1` |
| CafeGuest | open | - |
| Neighbor24 | WPA2 | `secret123` |
| PopUpHotspot | WPA2 | `popup5678` |

A real driver plugs in by filling in a `wifi::Backend` (`present`, `scan`,
`connect`, `disconnect`) and passing it to `wifi::manager().init()` in
`wifi_startup()` (`shell/src/apps_gui.cpp`). Nothing else changes.

## Saved networks

Stored in `/system/WIFI.CFG`, one record per line:
`<ssid length>,<passphrase length>,<ssid><passphrase>`. Without a mounted disk
nothing persists and the screen says so.

**The passphrase is stored in clear text and the Files app can open the file.**
That is a placeholder until Zanop has a way to protect secrets; do not treat it
as secure.

## Limits / not done

- No real adapter, 802.11 or WPA handshake code; no hidden networks, WPA3 or
  enterprise (802.1X) networks, no IP configuration (DHCP) after joining.
- Eight networks are shown and eight are remembered (the oldest is dropped).
- Arrow keys are not decoded by the keyboard driver yet, so selection is by number.

## Tests

```
g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -Ishell/src \
    tests/wifi/test_wifi.cpp -o /tmp/test_wifi && /tmp/test_wifi
```
The manager (`shell/src/wifi.hpp`) is header-only and free of kernel headers so
it can be tested on the host.
