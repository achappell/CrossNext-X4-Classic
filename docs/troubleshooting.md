# Troubleshooting

This document shows common issues and possible solutions while using the device features.

- [Troubleshooting](#troubleshooting)
    - [Cannot See the Device on the Network](#cannot-see-the-device-on-the-network)
    - [Connection Drops or Times Out](#connection-drops-or-times-out)
    - [Upload Fails](#upload-fails)
    - [Saved Password Not Working](#saved-password-not-working)

### Cannot See the Device on the Network

**Problem:** Browser shows "Cannot connect" or "Site can't be reached"

> **On CrossPlay these names differ.** The device advertises `crossplay.local`, not `crosspoint.local`, and the hotspot it creates is called `CrossPlay`, not `CrossPoint-Reader`. This page is upstream's and is kept unedited so merges stay cheap; substitute as you read.

**Solutions:**

1. Verify both devices are on the correct network
   - Check your computer/phone Wi-Fi settings
   - In **Join Network** mode, your computer/phone and CrossPoint Reader must be on the same Wi-Fi network
   - In **Create Hotspot** mode, your computer/phone must be connected to the `CrossPoint-Reader` hotspot
2. Double-check the IP address
   - Make sure you typed it correctly
   - Include `http://` at the beginning
   - Try the displayed IP address if `http://crosspoint.local/` does not resolve
3. Try disabling VPN if you're using one
4. Some networks have "client isolation" enabled - use Create Hotspot mode or check with your network administrator

### Connection Drops or Times Out

**Problem:** Wi-Fi connection is unstable

**Solutions:**

1. Move closer to the Wi-Fi router, or use Create Hotspot mode for a direct connection
2. Check signal strength on the device (should be at least `||` or better)
3. Avoid interference from other devices
4. Try a different Wi-Fi network if available

#### Opt-in Classic WiFi diagnostics

Only `x4c_wifi_diagnostics` and `gh_release_x4c_wifi_diagnostics` enable this
observer, and compilation also requires `FREEINK_DEVICE_X4CLASSIC`. Ordinary
Classic, Pro and C3 builds do not register an observer or write diagnostics.
This prepares a trace, not a connection fix or hardware verification.

While the WiFi picker owns the radio, at most 128 numeric records are retained
in RAM. On picker exit, capture stops before storage work and radio ownership
resumes only afterwards. Publication writes at most 16 KiB to
`/.crosspoint/classic-wifi-diagnostic.tmp`, checks byte counts and close, then
uses `replaceFile` for `/.crosspoint/classic-wifi-diagnostic.log`. Only these
owned files are touched. Latest-session publication replaces the previous trace;
storage failure is best effort with no diagnostic retry. File presence is not
freshness: failed publication may leave a previous session's log. Replacement
removes the destination before renaming, so a later failure can instead lose the
old log; this is not atomic replacement. Flush has no durability result, and
reset/power loss before exit loses the RAM capture. Overflow drops later records, including terminal
records, and is explicitly reported in the final summary.

Schema version 1 begins `classic-wifi-diagnostic,1,delivery-window`. Each record
is `session_elapsed_ms,attempt_ordinal,attempt_elapsed_ms,kind,value,extra`.
Unsigned elapsed subtraction tolerates one millisecond-counter wrap. Attempt zero
means no active attempt. Kind values are: 1 session; 2 attempt (source in value,
automatic boolean in extra; a second record carries timeout milliseconds with
extra 2); 3 mode return boolean; 4 disconnect return boolean; 5 synchronous begin
status enum; 6 changed polled status enum; 7 outcome; 8 station event; 9 end.
Sources: 1 remembered, 2 scanned, 3 manually entered. Outcomes: 1 connected,
2 failed, 3 timeout, 4 cancelled, 5 session ended. Station values: 1 START,
2 STOP, 3 associated, 4 disconnected (numeric SDK reason in extra), 5 GOT_IP
(extra 1), 6 LOST_IP (extra 0). Session value 1 identifies entry. Other unused
fields are zero. Final `summary,count,dropped,elapsed_ms,incomplete,final_attempt,final_outcome`
survives record-buffer overflow; drop count saturates at UINT32_MAX. Final attempt
and outcome are retained separately even if terminal records overflow.

Station attribution means **active at callback delivery**, not proven radio
generation: a late event may appear under a newer attempt or be ignored outside
the picker. No activity pointer or event payload is retained. Passwords,
credential values/lengths, SSID/BSSID/MAC, IP addresses, packets and general log
strings are excluded.

For later retrieval, leave the picker normally, then use **File Transfer →
USB Drive**, copy the log, eject the drive and reboot normally. Do not attempt
filesystem writes while USB Drive owns the card. Physical SD reliability and USB
export are unverified; no installation is authorized by these build targets.

The reviewed local application-only delivery is in ignored `dist/wifi-diagnostics/`
with its own manifest/hash and notes, separate from normal firmware packages.
The final diagnostic host suite (including production-method consumer and actual
temporary filesystem publication) and ordinary/diagnostic Classic dev/release
builds pass. Offline checks confirm S3/x4c identity, diagnostic-marker opt-in and
fit in both measured 8,257,536-byte installed application slots. These software
checks do not validate physical connectivity, SD durability or USB export.

### Upload Fails

**Problem:** File upload doesn't complete or shows an error

**Solutions:**

1. Check that the SD card has enough free space
2. Check that the filename is valid for the SD card filesystem
3. Try uploading a smaller file first to test
4. Refresh the browser page and try again
5. If WebSocket upload fails repeatedly, refresh the page and retry with the HTTP fallback path

### Saved Password Not Working

**Problem:** Device fails to connect with saved credentials

**Solutions:**

1. When connection fails, you'll be prompted to "Forget Network"
2. Select **Yes** to remove the saved password
3. Reconnect and enter the password again
4. Choose to save the new password
