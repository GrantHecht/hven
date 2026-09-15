# Every foreign pid of every batch, with every state observed for it

Settler ruling R13 (W5 T8.9r fix round 3): R2' asks for every foreign pid and state seen in any snapshot, listed -- not a count and a busiest-five. This is that list, written by `scripts/idle_proof.py` from the same snapshots the fractions are computed from.

States are the UNION of `/proc/<pid>/stat`'s single character (`FOREIGN_TICK`) and `ps`'s full string (`FOREIGN_PS`). A pid marked **transient** was present in some snapshot of the batch but not in both ends, so it has no CPU delta. `delta` is ticks/clk across the batch window for pids present at both ends.

## `E3-perf.log` / `e3`  (2 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.00 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.02 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.01 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.01 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.01 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `S,Sl` | 1.95 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.01 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.04 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.02 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.11 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.71 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.02 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.02 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.30 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.07 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.45 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.01 | both ends | `bash` |
| 3116480 | `S` | - | **transient** | `systemd-userwor` |
| 3116528 | `S` | - | **transient** | `systemd-userwor` |
| 3116543 | `S` | - | **transient** | `systemd-userwor` |
| 3121392 | `S,SN` | - | **transient** | `sleep` |
| 3121408 | `S,SN` | - | **transient** | `sleep` |
| 3121410 | `S,SN` | - | **transient** | `sleep` |
| 3121414 | `S,SN` | - | **transient** | `sleep` |
| 3121418 | `S,SN` | - | **transient** | `sleep` |
| 3121420 | `S,SN` | - | **transient** | `sleep` |
| 3121424 | `S,SN` | - | **transient** | `sleep` |
| 3121425 | `S,SN` | - | **transient** | `sleep` |
| 3121428 | `S,SN` | - | **transient** | `sleep` |
| 3121430 | `S,SN` | - | **transient** | `sleep` |
| 3121433 | `S,SN` | - | **transient** | `sleep` |
| 3121436 | `S,SN` | - | **transient** | `sleep` |
| 3121438 | `S,SN` | - | **transient** | `sleep` |
| 3121440 | `S,SN` | - | **transient** | `sleep` |
| 3122197 | `S` | - | **transient** | `systemd-userwor` |
| 3122249 | `S` | - | **transient** | `systemd-userwor` |
| 3122296 | `S` | - | **transient** | `systemd-userwor` |
| 3122310 | `S,SN` | - | **transient** | `sleep` |
| 3122336 | `S,SN` | - | **transient** | `sleep` |
| 3122341 | `S,SN` | - | **transient** | `sleep` |
| 3122373 | `S,SN` | - | **transient** | `sleep` |
| 3122379 | `S,SN` | - | **transient** | `sleep` |
| 3122382 | `S,SN` | - | **transient** | `sleep` |
| 3122383 | `S,SN` | - | **transient** | `sleep` |
| 3122385 | `S,SN` | - | **transient** | `sleep` |
| 3122401 | `S,SN` | - | **transient** | `sleep` |
| 3122423 | `S,SN` | - | **transient** | `sleep` |
| 3122425 | `S,SN` | - | **transient** | `sleep` |
| 3122427 | `S,SN` | - | **transient** | `sleep` |
| 3122429 | `S,SN` | - | **transient** | `sleep` |
| 3122432 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.23 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P1-pf-r1.log` / `pf-r1`  (2 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.00 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.00 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.02 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.00 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.01 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.01 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `S,Sl` | 1.59 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.03 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.01 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.01 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.08 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.58 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.02 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.03 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.28 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.16 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.25 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.01 | both ends | `bash` |
| 3116480 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3116528 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3116543 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3116569 | `S,SN` | - | **transient** | `sleep` |
| 3116592 | `S,SN` | - | **transient** | `sleep` |
| 3116595 | `S,SN` | - | **transient** | `sleep` |
| 3116597 | `S,SN` | - | **transient** | `sleep` |
| 3116601 | `S,SN` | - | **transient** | `sleep` |
| 3116617 | `S,SN` | - | **transient** | `sleep` |
| 3116619 | `S,SN` | - | **transient** | `sleep` |
| 3116623 | `S,SN` | - | **transient** | `sleep` |
| 3116626 | `S,SN` | - | **transient** | `sleep` |
| 3116628 | `S,SN` | - | **transient** | `sleep` |
| 3116639 | `S,SN` | - | **transient** | `sleep` |
| 3116640 | `S,SN` | - | **transient** | `sleep` |
| 3116642 | `S,SN` | - | **transient** | `sleep` |
| 3116647 | `S,SN` | - | **transient** | `sleep` |
| 3117391 | `S,SN` | - | **transient** | `sleep` |
| 3117413 | `S,SN` | - | **transient** | `sleep` |
| 3117417 | `S,SN` | - | **transient** | `sleep` |
| 3117451 | `S,SN` | - | **transient** | `sleep` |
| 3117453 | `S,SN` | - | **transient** | `sleep` |
| 3117455 | `S,SN` | - | **transient** | `sleep` |
| 3117476 | `S,SN` | - | **transient** | `sleep` |
| 3117478 | `S,SN` | - | **transient** | `sleep` |
| 3117494 | `S,SN` | - | **transient** | `sleep` |
| 3117496 | `S,SN` | - | **transient** | `sleep` |
| 3117498 | `S,SN` | - | **transient** | `sleep` |
| 3117520 | `S,SN` | - | **transient** | `sleep` |
| 3117522 | `S,SN` | - | **transient** | `sleep` |
| 3117526 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.16 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P2-pf-r2.log` / `pf-r2`  (2 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.02 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.02 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.01 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.01 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.00 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.01 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `R,S,Sl` | 1.67 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.03 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.01 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.02 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.09 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.58 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.02 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.03 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.33 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.08 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.27 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.00 | both ends | `bash` |
| 3116480 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3116528 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3116543 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3117391 | `S,SN` | - | **transient** | `sleep` |
| 3117413 | `S,SN` | - | **transient** | `sleep` |
| 3117417 | `S,SN` | - | **transient** | `sleep` |
| 3117451 | `S,SN` | - | **transient** | `sleep` |
| 3117453 | `S,SN` | - | **transient** | `sleep` |
| 3117455 | `S,SN` | - | **transient** | `sleep` |
| 3117476 | `S,SN` | - | **transient** | `sleep` |
| 3117478 | `S,SN` | - | **transient** | `sleep` |
| 3117494 | `S,SN` | - | **transient** | `sleep` |
| 3117496 | `S,SN` | - | **transient** | `sleep` |
| 3117498 | `S,SN` | - | **transient** | `sleep` |
| 3117520 | `S,SN` | - | **transient** | `sleep` |
| 3117522 | `S,SN` | - | **transient** | `sleep` |
| 3117526 | `S,SN` | - | **transient** | `sleep` |
| 3118985 | `S,SN` | - | **transient** | `sleep` |
| 3118987 | `SN` | - | **transient** | `sleep` |
| 3118991 | `S,SN` | - | **transient** | `sleep` |
| 3119046 | `S,SN` | - | **transient** | `sleep` |
| 3119048 | `S,SN` | - | **transient** | `sleep` |
| 3119050 | `S,SN` | - | **transient** | `sleep` |
| 3119071 | `S,SN` | - | **transient** | `sleep` |
| 3119073 | `S,SN` | - | **transient** | `sleep` |
| 3119090 | `S,SN` | - | **transient** | `sleep` |
| 3119092 | `S,SN` | - | **transient** | `sleep` |
| 3119094 | `S,SN` | - | **transient** | `sleep` |
| 3119096 | `S,SN` | - | **transient** | `sleep` |
| 3119100 | `S,SN` | - | **transient** | `sleep` |
| 3119107 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.17 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P3-pf-r3.log` / `pf-r3`  (2 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.00 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.01 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.00 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.01 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.01 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.02 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.00 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.01 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.00 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.00 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `S,Sl` | 1.53 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.03 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.01 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.02 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.08 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.50 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.02 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.02 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.28 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.07 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.25 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.00 | both ends | `bash` |
| 3116480 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3116528 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3116543 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3118985 | `S,SN` | - | **transient** | `sleep` |
| 3118991 | `S,SN` | - | **transient** | `sleep` |
| 3119046 | `S,SN` | - | **transient** | `sleep` |
| 3119048 | `S,SN` | - | **transient** | `sleep` |
| 3119050 | `S,SN` | - | **transient** | `sleep` |
| 3119071 | `S,SN` | - | **transient** | `sleep` |
| 3119073 | `S,SN` | - | **transient** | `sleep` |
| 3119090 | `S,SN` | - | **transient** | `sleep` |
| 3119092 | `S,SN` | - | **transient** | `sleep` |
| 3119094 | `SN` | - | **transient** | `sleep` |
| 3119096 | `S,SN` | - | **transient** | `sleep` |
| 3119100 | `S,SN` | - | **transient** | `sleep` |
| 3119107 | `S,SN` | - | **transient** | `sleep` |
| 3119710 | `S,SN` | - | **transient** | `sleep` |
| 3120535 | `S,SN` | - | **transient** | `sleep` |
| 3120557 | `S,SN` | - | **transient** | `sleep` |
| 3120561 | `S,SN` | - | **transient** | `sleep` |
| 3120578 | `S,SN` | - | **transient** | `sleep` |
| 3120584 | `S,SN` | - | **transient** | `sleep` |
| 3120586 | `S,SN` | - | **transient** | `sleep` |
| 3120587 | `S,SN` | - | **transient** | `sleep` |
| 3120589 | `S,SN` | - | **transient** | `sleep` |
| 3120605 | `S,SN` | - | **transient** | `sleep` |
| 3120627 | `S,SN` | - | **transient** | `sleep` |
| 3120629 | `S,SN` | - | **transient** | `sleep` |
| 3120631 | `S,SN` | - | **transient** | `sleep` |
| 3120634 | `S,SN` | - | **transient** | `sleep` |
| 3120641 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.15 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W1-wall-r1.log` / `wall-r1`  (6 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.00 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.01 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.00 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.00 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.00 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `R,S,Sl` | 1.06 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.01 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.02 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.05 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.36 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.09 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.08 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.28 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.00 | both ends | `bash` |
| 3087234 | `S` | - | **transient** | `systemd-userwor` |
| 3087268 | `S` | - | **transient** | `systemd-userwor` |
| 3087279 | `S` | - | **transient** | `systemd-userwor` |
| 3087791 | `S,SN` | - | **transient** | `sleep` |
| 3087793 | `S,SN` | - | **transient** | `sleep` |
| 3087795 | `S,SN` | - | **transient** | `sleep` |
| 3087799 | `S,SN` | 0.00 | both ends | `sleep` |
| 3087803 | `S,SN` | - | **transient** | `sleep` |
| 3087805 | `S,SN` | - | **transient** | `sleep` |
| 3087807 | `S,SN` | - | **transient** | `sleep` |
| 3087809 | `SN` | - | **transient** | `sleep` |
| 3087812 | `S,SN` | - | **transient** | `sleep` |
| 3087815 | `S,SN` | - | **transient** | `sleep` |
| 3087824 | `S,SN` | - | **transient** | `sleep` |
| 3087826 | `S,SN` | - | **transient** | `sleep` |
| 3087828 | `S,SN` | - | **transient** | `sleep` |
| 3087891 | `S,SN` | - | **transient** | `sleep` |
| 3088184 | `S,SN` | - | **transient** | `sleep` |
| 3088527 | `S,SN` | - | **transient** | `sleep` |
| 3088529 | `S,SN` | - | **transient** | `sleep` |
| 3088531 | `S,SN` | - | **transient** | `sleep` |
| 3088533 | `S,SN` | - | **transient** | `sleep` |
| 3088535 | `S,SN` | - | **transient** | `sleep` |
| 3088536 | `S` | - | **transient** | `systemd-userwor` |
| 3089198 | `S,SN` | - | **transient** | `sleep` |
| 3089208 | `S,SN` | - | **transient** | `sleep` |
| 3089210 | `S,SN` | - | **transient** | `sleep` |
| 3089256 | `S,SN` | - | **transient** | `sleep` |
| 3089878 | `S,SN` | - | **transient** | `sleep` |
| 3089880 | `S,SN` | - | **transient** | `sleep` |
| 3089900 | `S,SN` | - | **transient** | `sleep` |
| 3089902 | `S` | - | **transient** | `systemd-userwor` |
| 3089904 | `S,SN` | - | **transient** | `sleep` |
| 3089906 | `S,SN` | - | **transient** | `sleep` |
| 3089913 | `S,SN` | - | **transient** | `sleep` |
| 3090590 | `S,SN` | - | **transient** | `sleep` |
| 3090592 | `S,SN` | - | **transient** | `sleep` |
| 3090593 | `S,SN` | - | **transient** | `sleep` |
| 3090594 | `S` | - | **transient** | `systemd-userwor` |
| 3090610 | `S,SN` | - | **transient** | `sleep` |
| 3090612 | `S,SN` | - | **transient** | `sleep` |
| 3090614 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.11 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W2-wall-r2.log` / `wall-r2`  (6 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.00 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.01 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.01 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.01 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.00 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.00 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `S,Sl` | 0.81 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.01 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.00 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.04 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.30 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.08 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.02 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.06 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.01 | both ends | `bash` |
| 3088536 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3089902 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3090594 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3108301 | `S,SN` | - | **transient** | `sleep` |
| 3110398 | `S,SN` | - | **transient** | `sleep` |
| 3110400 | `S,SN` | - | **transient** | `sleep` |
| 3110403 | `SN` | - | **transient** | `sleep` |
| 3111144 | `S,SN` | - | **transient** | `sleep` |
| 3111308 | `S,SN` | - | **transient** | `sleep` |
| 3112387 | `S,SN` | - | **transient** | `sleep` |
| 3112390 | `S,SN` | - | **transient** | `sleep` |
| 3112391 | `S,SN` | - | **transient** | `sleep` |
| 3112393 | `S,SN` | - | **transient** | `sleep` |
| 3112409 | `S,SN` | - | **transient** | `sleep` |
| 3112412 | `S,SN` | - | **transient** | `sleep` |
| 3112413 | `S,SN` | - | **transient** | `sleep` |
| 3112415 | `S,SN` | - | **transient** | `sleep` |
| 3112577 | `S,SN` | - | **transient** | `sleep` |
| 3113100 | `S,SN` | - | **transient** | `sleep` |
| 3113102 | `S,SN` | - | **transient** | `sleep` |
| 3113104 | `S,SN` | - | **transient** | `sleep` |
| 3113567 | `S,SN` | - | **transient** | `sleep` |
| 3113779 | `S,SN` | - | **transient** | `sleep` |
| 3113781 | `S,SN` | - | **transient** | `sleep` |
| 3113793 | `S,SN` | - | **transient** | `sleep` |
| 3114471 | `S,SN` | - | **transient** | `sleep` |
| 3114474 | `S,SN` | - | **transient** | `sleep` |
| 3114476 | `S,SN` | - | **transient** | `sleep` |
| 3114478 | `S,SN` | - | **transient** | `sleep` |
| 3115154 | `S,SN` | - | **transient** | `sleep` |
| 3115156 | `S,SN` | - | **transient** | `sleep` |
| 3115172 | `S,SN` | - | **transient** | `sleep` |
| 3115174 | `S,SN` | - | **transient** | `sleep` |
| 3115176 | `S,SN` | - | **transient** | `sleep` |
| 3115179 | `S,SN` | - | **transient** | `sleep` |
| 3115347 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.10 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W3-wall-r3.log` / `wall-r3`  (6 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.00 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.01 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.01 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.00 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.00 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.01 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.01 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `S,Sl` | 0.83 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.02 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.02 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.01 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.05 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.01 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.28 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.07 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.06 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.34 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.11 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `Rl,S,Sl` | 0.12 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.00 | both ends | `bash` |
| 3088536 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3089902 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3090594 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3092614 | `S,SN` | - | **transient** | `sleep` |
| 3093293 | `S,SN` | - | **transient** | `sleep` |
| 3093295 | `S,SN` | - | **transient** | `sleep` |
| 3093983 | `S,SN` | - | **transient** | `sleep` |
| 3093985 | `S,SN` | - | **transient** | `sleep` |
| 3093987 | `S,SN` | - | **transient** | `sleep` |
| 3094664 | `S,SN` | - | **transient** | `sleep` |
| 3094665 | `S,SN` | - | **transient** | `sleep` |
| 3094682 | `S,SN` | - | **transient** | `sleep` |
| 3094686 | `S,SN` | - | **transient** | `sleep` |
| 3094688 | `S,SN` | - | **transient** | `sleep` |
| 3094690 | `S,SN` | - | **transient** | `sleep` |
| 3095994 | `S,SN` | - | **transient** | `sleep` |
| 3096030 | `S,SN` | - | **transient** | `sleep` |
| 3096679 | `S,SN` | - | **transient** | `sleep` |
| 3096681 | `S,SN` | - | **transient** | `sleep` |
| 3096682 | `S,SN` | - | **transient** | `sleep` |
| 3096684 | `S,SN` | - | **transient** | `sleep` |
| 3097361 | `S,SN` | - | **transient** | `sleep` |
| 3097364 | `S,SN` | - | **transient** | `sleep` |
| 3097366 | `S,SN` | - | **transient** | `sleep` |
| 3097378 | `S,SN` | - | **transient** | `sleep` |
| 3098056 | `S,SN` | - | **transient** | `sleep` |
| 3098058 | `S,SN` | - | **transient** | `sleep` |
| 3098059 | `S,SN` | - | **transient** | `sleep` |
| 3098075 | `S,SN` | - | **transient** | `sleep` |
| 3098077 | `S,SN` | - | **transient** | `sleep` |
| 3098079 | `S,SN` | - | **transient** | `sleep` |
| 3098130 | `S,SN` | - | **transient** | `sleep` |
| 3098739 | `S,SN` | - | **transient** | `sleep` |
| 3098741 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.11 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W4-wall-r4.log` / `wall-r4`  (6 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.00 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.00 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.00 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.00 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.00 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `Rl,S,Sl` | 0.81 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.01 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.01 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.04 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.27 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.05 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.05 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.02 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.00 | both ends | `bash` |
| 3088536 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3089902 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3090594 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3096679 | `S,SN` | - | **transient** | `sleep` |
| 3096681 | `S,SN` | - | **transient** | `sleep` |
| 3097364 | `S,SN` | - | **transient** | `sleep` |
| 3097366 | `S,SN` | - | **transient** | `sleep` |
| 3097378 | `S,SN` | - | **transient** | `sleep` |
| 3098056 | `S,SN` | - | **transient** | `sleep` |
| 3098058 | `S,SN` | - | **transient** | `sleep` |
| 3098059 | `S,SN` | - | **transient** | `sleep` |
| 3098075 | `S,SN` | - | **transient** | `sleep` |
| 3098079 | `S,SN` | - | **transient** | `sleep` |
| 3098130 | `S,SN` | - | **transient** | `sleep` |
| 3098739 | `S,SN` | - | **transient** | `sleep` |
| 3098741 | `S,SN` | 0.00 | both ends | `sleep` |
| 3100089 | `S,SN` | - | **transient** | `sleep` |
| 3100738 | `S,SN` | - | **transient** | `sleep` |
| 3100740 | `S,SN` | - | **transient** | `sleep` |
| 3100742 | `S,SN` | - | **transient** | `sleep` |
| 3100743 | `S,SN` | - | **transient** | `sleep` |
| 3100746 | `S,SN` | - | **transient** | `sleep` |
| 3101424 | `S,SN` | - | **transient** | `sleep` |
| 3101428 | `S,SN` | - | **transient** | `sleep` |
| 3101430 | `S,SN` | - | **transient** | `sleep` |
| 3101432 | `S,SN` | - | **transient** | `sleep` |
| 3102093 | `S,SN` | - | **transient** | `sleep` |
| 3102094 | `S,SN` | - | **transient** | `sleep` |
| 3102111 | `S,SN` | - | **transient** | `sleep` |
| 3102113 | `S,SN` | - | **transient** | `sleep` |
| 3102115 | `S,SN` | - | **transient** | `sleep` |
| 3102117 | `S,SN` | - | **transient** | `sleep` |
| 3102794 | `S,SN` | - | **transient** | `sleep` |
| 3102796 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.10 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W5-wall-r5.log` / `wall-r5`  (6 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.00 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.00 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.01 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.00 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.02 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.01 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.00 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.00 | both ends | `obexd` |
| 2405 | `S,Ssl` | 0.00 | both ends | `abrt-applet` |
| 2423 | `S,Ssl` | 0.00 | both ends | `kunifiedpush-di` |
| 2429 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 2437 | `S,Ssl` | 0.00 | both ends | `agent` |
| 2469 | `S,Ssl` | 0.00 | both ends | `xdg-permission-` |
| 2492 | `S,Ssl` | 0.00 | both ends | `xdg-document-po` |
| 2519 | `S,Ss` | 0.00 | both ends | `fusermount3` |
| 2539 | `S,Ssl` | 0.00 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.01 | both ends | `tmux:` |
| 4095 | `S,Ss+` | 0.00 | both ends | `fish` |
| 5005 | `S,Ss` | 0.00 | both ends | `fish` |
| 37066 | `S,Ss` | 0.00 | both ends | `login` |
| 38851 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 41738 | `S,Ssl` | 0.00 | both ends | `sddm` |
| 41777 | `S,Ss+` | 0.00 | both ends | `agetty` |
| 42197 | `S,Ss+` | 0.00 | both ends | `fish` |
| 49717 | `S` | 0.00 | both ends | `sddm-helper` |
| 49724 | `S,SLl` | 0.00 | both ends | `ksecretd` |
| 49725 | `S,Ssl+` | 0.00 | both ends | `startplasma-way` |
| 50099 | `S,Ssl` | 0.00 | both ends | `kdeconnectd` |
| 50103 | `S,Ssl` | 0.00 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `S,Sl` | 0.94 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.01 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.01 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.00 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.00 | both ends | `xembedsniproxy` |
| 50732 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50858 | `S` | 0.00 | both ends | `xsettingsd` |
| 50972 | `S,Sl` | 0.00 | both ends | `akonadi_archive` |
| 50973 | `S,Sl` | 0.00 | both ends | `akonadi_birthda` |
| 50978 | `S,Sl` | 0.00 | both ends | `akonadi_contact` |
| 50979 | `S,Sl` | 0.00 | both ends | `akonadi_followu` |
| 50981 | `S,Sl` | 0.00 | both ends | `akonadi_ical_re` |
| 50983 | `S,SNl` | 0.00 | both ends | `akonadi_indexin` |
| 50984 | `S,Sl` | 0.00 | both ends | `akonadi_maildir` |
| 50985 | `S,Sl` | 0.00 | both ends | `akonadi_maildis` |
| 50986 | `S,Sl` | 0.00 | both ends | `akonadi_mailfil` |
| 50987 | `S,Sl` | 0.00 | both ends | `akonadi_mailmer` |
| 50988 | `S,Sl` | 0.00 | both ends | `akonadi_migrati` |
| 50989 | `S,Sl` | 0.00 | both ends | `akonadi_newmail` |
| 50990 | `S,Sl` | 0.00 | both ends | `akonadi_sendlat` |
| 50991 | `S,Sl` | 0.00 | both ends | `akonadi_unified` |
| 58901 | `S,Ssl` | 0.00 | both ends | `baloorunner` |
| 58924 | `S,Ssl` | 0.00 | both ends | `fwupd` |
| 58989 | `S,Ssl` | 0.00 | both ends | `passimd` |
| 59401 | `S,Ssl` | 0.00 | both ends | `krunner` |
| 59455 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59463 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59465 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59466 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.04 | both ends | `kscreenlocker_g` |
| 118500 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 128718 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 133876 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 146228 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 152284 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 164863 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 167025 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 206450 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209083 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 209293 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 216768 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `Rl+,S,Sl+` | 0.32 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.06 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 3086570 | `S,SNs` | 0.00 | both ends | `bash` |
| 3088536 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3089902 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3090594 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3098741 | `S,SN` | - | **transient** | `sleep` |
| 3100738 | `S,SN` | - | **transient** | `sleep` |
| 3100740 | `S,SN` | - | **transient** | `sleep` |
| 3101428 | `S,SN` | - | **transient** | `sleep` |
| 3101430 | `S,SN` | - | **transient** | `sleep` |
| 3101432 | `S,SN` | - | **transient** | `sleep` |
| 3102093 | `S,SN` | - | **transient** | `sleep` |
| 3102094 | `S,SN` | - | **transient** | `sleep` |
| 3102111 | `S,SN` | - | **transient** | `sleep` |
| 3102113 | `SN` | - | **transient** | `sleep` |
| 3102115 | `S,SN` | - | **transient** | `sleep` |
| 3102117 | `S,SN` | - | **transient** | `sleep` |
| 3102794 | `S,SN` | - | **transient** | `sleep` |
| 3102796 | `S,SN` | - | **transient** | `sleep` |
| 3104196 | `S,SN` | - | **transient** | `sleep` |
| 3104794 | `S,SN` | - | **transient** | `sleep` |
| 3104796 | `S,SN` | - | **transient** | `sleep` |
| 3104797 | `S,SN` | - | **transient** | `sleep` |
| 3104799 | `S,SN` | - | **transient** | `sleep` |
| 3105460 | `S,SN` | - | **transient** | `sleep` |
| 3105463 | `S,SN` | - | **transient** | `sleep` |
| 3105465 | `S,SN` | - | **transient** | `sleep` |
| 3105472 | `S,SN` | - | **transient** | `sleep` |
| 3106149 | `S,SN` | - | **transient** | `sleep` |
| 3106151 | `S,SN` | - | **transient** | `sleep` |
| 3106152 | `S,SN` | - | **transient** | `sleep` |
| 3106168 | `S,SN` | - | **transient** | `sleep` |
| 3106170 | `S,SN` | - | **transient** | `sleep` |
| 3106172 | `S,SN` | - | **transient** | `sleep` |
| 3106174 | `S,SN` | - | **transient** | `sleep` |
| 3106851 | `S,SN` | - | **transient** | `sleep` |
| 3106853 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.10 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |
