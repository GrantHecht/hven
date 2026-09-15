# Every foreign pid of every batch, with every state observed for it

Settler ruling R13 (W5 T8.9r fix round 3): R2' asks for every foreign pid and state seen in any snapshot, listed -- not a count and a busiest-five. This is that list, written by `scripts/idle_proof.py` from the same snapshots the fractions are computed from.

States are the UNION of `/proc/<pid>/stat`'s single character (`FOREIGN_TICK`) and `ps`'s full string (`FOREIGN_PS`). A pid marked **transient** was present in some snapshot of the batch but not in both ends, so it has no CPU delta. `delta` is ticks/clk across the batch window for pids present at both ends.

## `D1-diff-r1.log` / `diff-r1`  (13 snapshots)

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
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.01 | both ends | `NetworkManager` |
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
| 50122 | `Rl,S,Sl` | 2.84 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
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
| 116643 | `S,Sl` | 0.13 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.93 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.19 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.16 | both ends | `codex` |
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
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.36 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.10 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.12 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.41 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2777647 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2777755 | `S,SN` | - | **transient** | `sleep` |
| 2777757 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2777758 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2777817 | `S,SN` | - | **transient** | `sleep` |
| 2777819 | `S,SN` | - | **transient** | `sleep` |
| 2777823 | `S,SN` | - | **transient** | `sleep` |
| 2777825 | `S,SN` | - | **transient** | `sleep` |
| 2777828 | `S,SN` | - | **transient** | `sleep` |
| 2777837 | `S,SN` | - | **transient** | `sleep` |
| 2777846 | `S,SN` | - | **transient** | `sleep` |
| 2777850 | `S,SN` | - | **transient** | `sleep` |
| 2777852 | `S,SN` | - | **transient** | `sleep` |
| 2777854 | `S,SN` | - | **transient** | `sleep` |
| 2777856 | `S,SN` | - | **transient** | `sleep` |
| 2777858 | `S,SN` | - | **transient** | `sleep` |
| 2778542 | `S,SN` | - | **transient** | `sleep` |
| 2778544 | `S,SN` | - | **transient** | `sleep` |
| 2778545 | `S,SN` | - | **transient** | `sleep` |
| 2778558 | `S,SN` | - | **transient** | `sleep` |
| 2779219 | `S,SN` | - | **transient** | `sleep` |
| 2779222 | `S,SN` | - | **transient** | `sleep` |
| 2779224 | `S,SN` | - | **transient** | `sleep` |
| 2779237 | `S,SN` | - | **transient** | `sleep` |
| 2779460 | `S,SN` | - | **transient** | `sleep` |
| 2779913 | `S,SN` | - | **transient** | `sleep` |
| 2779921 | `S,SN` | - | **transient** | `sleep` |
| 2779923 | `S,SN` | - | **transient** | `sleep` |
| 2780104 | `S,SN` | - | **transient** | `sleep` |
| 2780594 | `S,SN` | - | **transient** | `sleep` |
| 2780597 | `S,SN` | - | **transient** | `sleep` |
| 2781269 | `S,SN` | - | **transient** | `sleep` |
| 2781271 | `S,SN` | - | **transient** | `sleep` |
| 2781272 | `S,SN` | - | **transient** | `sleep` |
| 2781274 | `S,SN` | - | **transient** | `sleep` |
| 2781276 | `S,SN` | - | **transient** | `sleep` |
| 2781948 | `S,SN` | - | **transient** | `sleep` |
| 2781950 | `S,SN` | - | **transient** | `sleep` |
| 2781956 | `S,SN` | - | **transient** | `sleep` |
| 2781965 | `S,SN` | - | **transient** | `sleep` |
| 2782644 | `S,SN` | - | **transient** | `sleep` |
| 2782647 | `S,SN` | - | **transient** | `sleep` |
| 2782649 | `S,SN` | - | **transient** | `sleep` |
| 2782662 | `S,SN` | - | **transient** | `sleep` |
| 2782826 | `S,SN` | - | **transient** | `sleep` |
| 2783322 | `S,SN` | - | **transient** | `sleep` |
| 2783328 | `S,SN` | - | **transient** | `sleep` |
| 2783497 | `S,SN` | - | **transient** | `sleep` |
| 2783999 | `S,SN` | - | **transient** | `sleep` |
| 2784001 | `S,SN` | - | **transient** | `sleep` |
| 2784003 | `S,SN` | - | **transient** | `sleep` |
| 2784405 | `S,SN` | - | **transient** | `sleep` |
| 2784676 | `S,SN` | - | **transient** | `sleep` |
| 2784678 | `S,SN` | - | **transient** | `sleep` |
| 2784680 | `S,SN` | - | **transient** | `sleep` |
| 2784690 | `S,SN` | - | **transient** | `sleep` |
| 2785368 | `S,SN` | - | **transient** | `sleep` |
| 2785370 | `S,SN` | - | **transient** | `sleep` |
| 2785372 | `S,SN` | - | **transient** | `sleep` |
| 2785374 | `S,SN` | - | **transient** | `sleep` |
| 2785392 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.29 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `D2-diff-r2.log` / `diff-r2`  (13 snapshots)

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
| 1244 | `S,Ssl` | 0.01 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 2.75 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.05 | both ends | `plasmashell` |
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
| 59463 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 116643 | `S,Sl` | 0.13 | both ends | `kscreenlocker_g` |
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
| 1097257 | `Rl+,S,Sl+` | 0.84 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.17 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.17 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.38 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.21 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.05 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.40 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2777647 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2777757 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2777758 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2783328 | `S,SN` | - | **transient** | `sleep` |
| 2786715 | `S,SN` | - | **transient** | `sleep` |
| 2786717 | `S,SN` | - | **transient** | `sleep` |
| 2786722 | `S,SN` | - | **transient** | `sleep` |
| 2786724 | `S,SN` | - | **transient** | `sleep` |
| 2786727 | `S,SN` | - | **transient** | `sleep` |
| 2786738 | `S,SN` | - | **transient** | `sleep` |
| 2786750 | `S,SN` | - | **transient** | `sleep` |
| 2786755 | `S,SN` | - | **transient** | `sleep` |
| 2786757 | `S,SN` | - | **transient** | `sleep` |
| 2786759 | `S,SN` | - | **transient** | `sleep` |
| 2786761 | `S,SN` | - | **transient** | `sleep` |
| 2786763 | `S,SN` | - | **transient** | `sleep` |
| 2787453 | `S,SN` | - | **transient** | `sleep` |
| 2787455 | `S,SN` | - | **transient** | `sleep` |
| 2787696 | `S,SN` | - | **transient** | `sleep` |
| 2788126 | `S,SN` | - | **transient** | `sleep` |
| 2788128 | `S,SN` | - | **transient** | `sleep` |
| 2788133 | `S,SN` | - | **transient** | `sleep` |
| 2788135 | `S,SN` | - | **transient** | `sleep` |
| 2788807 | `S,SN` | - | **transient** | `sleep` |
| 2788809 | `S,SN` | - | **transient** | `sleep` |
| 2788819 | `S,SN` | - | **transient** | `sleep` |
| 2788838 | `S,SN` | - | **transient** | `sleep` |
| 2789499 | `S,SN` | - | **transient** | `sleep` |
| 2789501 | `S,SN` | - | **transient** | `sleep` |
| 2789503 | `S,SN` | - | **transient** | `sleep` |
| 2789505 | `S,SN` | - | **transient** | `sleep` |
| 2790180 | `S,SN` | - | **transient** | `sleep` |
| 2790181 | `S,SN` | - | **transient** | `sleep` |
| 2790183 | `S,SN` | - | **transient** | `sleep` |
| 2790196 | `S,SN` | - | **transient** | `sleep` |
| 2790857 | `S,SN` | - | **transient** | `sleep` |
| 2790859 | `S,SN` | - | **transient** | `sleep` |
| 2790861 | `S,SN` | - | **transient** | `sleep` |
| 2790875 | `S,SN` | - | **transient** | `sleep` |
| 2791552 | `S,SN` | - | **transient** | `sleep` |
| 2791563 | `S,SN` | - | **transient** | `sleep` |
| 2791566 | `S,SN` | - | **transient** | `sleep` |
| 2791568 | `S,SN` | - | **transient** | `sleep` |
| 2792240 | `S,SN` | - | **transient** | `sleep` |
| 2792242 | `S,SN` | - | **transient** | `sleep` |
| 2792244 | `S,SN` | - | **transient** | `sleep` |
| 2792258 | `S,SN` | - | **transient** | `sleep` |
| 2792919 | `S,SN` | - | **transient** | `sleep` |
| 2792921 | `S,SN` | - | **transient** | `sleep` |
| 2792924 | `S,SN` | - | **transient** | `sleep` |
| 2792926 | `S,SN` | - | **transient** | `sleep` |
| 2793599 | `S,SN` | - | **transient** | `sleep` |
| 2793601 | `S,SN` | - | **transient** | `sleep` |
| 2793603 | `S,SN` | - | **transient** | `sleep` |
| 2793613 | `S,SN` | - | **transient** | `sleep` |
| 2793630 | `S,SN` | - | **transient** | `sleep` |
| 2794333 | `S,SN` | - | **transient** | `sleep` |
| 2794335 | `S,SN` | - | **transient** | `sleep` |
| 2794337 | `S,SN` | - | **transient** | `sleep` |
| 2794350 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.27 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `D3-diff-r3.log` / `diff-r3`  (13 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.01 | both ends | `NetworkManager` |
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
| 4094 | `S,Ss` | 0.02 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 2.76 | both ends | `kwin_wayland` |
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
| 50651 | `S,Ssl` | 0.01 | both ends | `xdg-desktop-por` |
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
| 116643 | `S,Sl` | 0.13 | both ends | `kscreenlocker_g` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.93 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.16 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.16 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.55 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.09 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.04 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.58 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2777647 | `S` | - | **transient** | `systemd-userwor` |
| 2777757 | `S` | - | **transient** | `systemd-userwor` |
| 2777758 | `S` | - | **transient** | `systemd-userwor` |
| 2792258 | `S,SN` | - | **transient** | `sleep` |
| 2792924 | `S,SN` | - | **transient** | `sleep` |
| 2792926 | `S,SN` | - | **transient** | `sleep` |
| 2793599 | `S,SN` | - | **transient** | `sleep` |
| 2793601 | `S,SN` | - | **transient** | `sleep` |
| 2793603 | `S,SN` | - | **transient** | `sleep` |
| 2793613 | `S,SN` | - | **transient** | `sleep` |
| 2793630 | `S,SN` | - | **transient** | `sleep` |
| 2794333 | `S,SN` | - | **transient** | `sleep` |
| 2794335 | `S,SN` | - | **transient** | `sleep` |
| 2794337 | `S,SN` | - | **transient** | `sleep` |
| 2794350 | `S,SN` | - | **transient** | `sleep` |
| 2795637 | `S,SN` | - | **transient** | `sleep` |
| 2796319 | `S,SN` | - | **transient** | `sleep` |
| 2796320 | `S,SN` | - | **transient** | `sleep` |
| 2796322 | `S,SN` | - | **transient** | `sleep` |
| 2796335 | `S,SN` | - | **transient** | `sleep` |
| 2796995 | `S` | - | **transient** | `systemd-userwor` |
| 2796997 | `S,SN` | - | **transient** | `sleep` |
| 2797000 | `S,SN` | - | **transient** | `sleep` |
| 2797002 | `S,SN` | - | **transient** | `sleep` |
| 2797016 | `S,SN` | - | **transient** | `sleep` |
| 2797691 | `S,SN` | - | **transient** | `sleep` |
| 2797695 | `S,SN` | - | **transient** | `sleep` |
| 2797697 | `S,SN` | - | **transient** | `sleep` |
| 2797699 | `S,SN` | - | **transient** | `sleep` |
| 2798373 | `S,SN` | - | **transient** | `sleep` |
| 2798375 | `S,SN` | - | **transient** | `sleep` |
| 2798377 | `S,SN` | - | **transient** | `sleep` |
| 2798391 | `S,SN` | - | **transient** | `sleep` |
| 2799051 | `S` | - | **transient** | `systemd-userwor` |
| 2799052 | `S` | - | **transient** | `systemd-userwor` |
| 2799054 | `S,SN` | - | **transient** | `sleep` |
| 2799055 | `S,SN` | - | **transient** | `sleep` |
| 2799058 | `S,SN` | - | **transient** | `sleep` |
| 2799060 | `S,SN` | - | **transient** | `sleep` |
| 2799734 | `S,SN` | - | **transient** | `sleep` |
| 2799736 | `S,SN` | - | **transient** | `sleep` |
| 2799738 | `S,SN` | - | **transient** | `sleep` |
| 2799748 | `S,SN` | - | **transient** | `sleep` |
| 2799757 | `S,SN` | - | **transient** | `sleep` |
| 2800428 | `S,SN` | - | **transient** | `sleep` |
| 2800430 | `S,SN` | - | **transient** | `sleep` |
| 2800432 | `S,SN` | - | **transient** | `sleep` |
| 2800445 | `S,SN` | - | **transient** | `sleep` |
| 2801106 | `S,SN` | - | **transient** | `sleep` |
| 2801109 | `S,SN` | - | **transient** | `sleep` |
| 2801781 | `S,SN` | - | **transient** | `sleep` |
| 2801783 | `S,SN` | - | **transient** | `sleep` |
| 2801785 | `S,SN` | - | **transient** | `sleep` |
| 2801788 | `S,SN` | - | **transient** | `sleep` |
| 2802403 | `S,SN` | - | **transient** | `sleep` |
| 2802491 | `S,SN` | - | **transient** | `sleep` |
| 2802494 | `S,SN` | - | **transient** | `sleep` |
| 2802510 | `S,SN` | - | **transient** | `sleep` |
| 2803184 | `S,SN` | - | **transient** | `sleep` |
| 2803186 | `S,SN` | - | **transient** | `sleep` |
| 2803188 | `S,SN` | - | **transient** | `sleep` |
| 2803191 | `S,SN` | - | **transient** | `sleep` |
| 2803193 | `S,SN` | - | **transient** | `sleep` |
| 2803206 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.25 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `D4-diff-r4.log` / `diff-r4`  (13 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
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
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.04 | both ends | `udisksd` |
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
| 1244 | `S,Ssl` | 0.10 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.01 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.02 | both ends | `rsyslogd` |
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
| 2539 | `S,Ssl` | 0.01 | both ends | `abrt-dbus` |
| 2745 | `S` | 0.00 | both ends | `UVM` |
| 2746 | `S` | 0.00 | both ends | `UVM` |
| 2747 | `S` | 0.00 | both ends | `UVM` |
| 3245 | `S` | 0.00 | both ends | `catatonit` |
| 4094 | `S,Ss` | 0.03 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 2.49 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
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
| 59662 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.14 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.96 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.17 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.15 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.54 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.09 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.36 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2834635 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2835397 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2835400 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2835622 | `S,Ssl` | - | **transient** | `tailscaled` |
| 2835629 | `S` | - | **transient** | `fish` |
| 2835761 | `S,S+` | - | **transient** | `tmux:` |
| 2835796 | `S,SN` | - | **transient** | `sleep` |
| 2835802 | `S,SN` | - | **transient** | `sleep` |
| 2835804 | `S,SN` | - | **transient** | `sleep` |
| 2835808 | `S,SN` | - | **transient** | `sleep` |
| 2835816 | `S,SN` | - | **transient** | `sleep` |
| 2835838 | `S,SN` | - | **transient** | `sleep` |
| 2835839 | `S,SN` | - | **transient** | `sleep` |
| 2835843 | `S,SN` | - | **transient** | `sleep` |
| 2835848 | `S,SN` | - | **transient** | `sleep` |
| 2835859 | `S,SN` | - | **transient** | `sleep` |
| 2835861 | `S,SN` | - | **transient** | `sleep` |
| 2835863 | `S,SN` | - | **transient** | `sleep` |
| 2835864 | `S,SN` | - | **transient** | `sleep` |
| 2836555 | `S,SN` | - | **transient** | `sleep` |
| 2836557 | `S,SN` | - | **transient** | `sleep` |
| 2836559 | `S,SN` | - | **transient** | `sleep` |
| 2836562 | `S,SN` | - | **transient** | `sleep` |
| 2837244 | `S,SN` | - | **transient** | `sleep` |
| 2837246 | `S,SN` | - | **transient** | `sleep` |
| 2837266 | `S,SN` | - | **transient** | `sleep` |
| 2837267 | `S,SN` | - | **transient** | `sleep` |
| 2837280 | `S,SN` | - | **transient** | `sleep` |
| 2837950 | `S,SN` | - | **transient** | `sleep` |
| 2837952 | `S,SN` | - | **transient** | `sleep` |
| 2837954 | `S,SN` | - | **transient** | `sleep` |
| 2837956 | `S,SN` | - | **transient** | `sleep` |
| 2838638 | `S,SN` | - | **transient** | `sleep` |
| 2838641 | `S,SN` | - | **transient** | `sleep` |
| 2838642 | `S,SN` | - | **transient** | `sleep` |
| 2838655 | `S,SN` | - | **transient** | `sleep` |
| 2839325 | `S,SN` | - | **transient** | `sleep` |
| 2839329 | `S,SN` | - | **transient** | `sleep` |
| 2839331 | `S,SN` | - | **transient** | `sleep` |
| 2839345 | `S,SN` | - | **transient** | `sleep` |
| 2840030 | `S,SN` | - | **transient** | `sleep` |
| 2840032 | `S,SN` | - | **transient** | `sleep` |
| 2840033 | `S,SN` | - | **transient** | `sleep` |
| 2840035 | `S,SN` | - | **transient** | `sleep` |
| 2840716 | `S,SN` | - | **transient** | `sleep` |
| 2840718 | `S,SN` | - | **transient** | `sleep` |
| 2840720 | `S,SN` | - | **transient** | `sleep` |
| 2841401 | `S,SN` | - | **transient** | `sleep` |
| 2841402 | `S,SN` | - | **transient** | `sleep` |
| 2841431 | `S,SN` | - | **transient** | `sleep` |
| 2841433 | `S,SN` | - | **transient** | `sleep` |
| 2841435 | `S,SN` | - | **transient** | `sleep` |
| 2842139 | `S,SN` | - | **transient** | `sleep` |
| 2842142 | `S,SN` | - | **transient** | `sleep` |
| 2842144 | `S,SN` | - | **transient** | `sleep` |
| 2842163 | `S,SN` | - | **transient** | `sleep` |
| 2842834 | `S,SN` | - | **transient** | `sleep` |
| 2842836 | `S,SN` | - | **transient** | `sleep` |
| 2842838 | `S,SN` | - | **transient** | `sleep` |
| 2842852 | `S,SN` | - | **transient** | `sleep` |
| 2843513 | `S,SN` | - | **transient** | `sleep` |
| 2843515 | `S,SN` | - | **transient** | `sleep` |
| 2843519 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.30 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.29 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `D5-diff-r5.log` / `diff-r5`  (13 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.00 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
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
| 1244 | `S,Ssl` | 0.03 | both ends | `tailscaled` |
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
| 1965 | `S,S<sl` | 0.01 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.02 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.01 | both ends | `at-spi2-registr` |
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
| 4094 | `S,Ss` | 0.02 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 2.54 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.05 | both ends | `plasmashell` |
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
| 58989 | `S,Ssl` | 0.01 | both ends | `passimd` |
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
| 116643 | `R,S,Sl` | 0.12 | both ends | `kscreenlocker_g` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.90 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.18 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.16 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.40 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.19 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.06 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.39 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2834635 | `S` | - | **transient** | `systemd-userwor` |
| 2835397 | `S` | - | **transient** | `systemd-userwor` |
| 2835400 | `S` | - | **transient** | `systemd-userwor` |
| 2841433 | `S,SN` | - | **transient** | `sleep` |
| 2841435 | `S,SN` | - | **transient** | `sleep` |
| 2842139 | `S,SN` | - | **transient** | `sleep` |
| 2842144 | `S,SN` | - | **transient** | `sleep` |
| 2842163 | `S,SN` | - | **transient** | `sleep` |
| 2842834 | `SN` | - | **transient** | `sleep` |
| 2842836 | `S,SN` | - | **transient** | `sleep` |
| 2842838 | `S,SN` | - | **transient** | `sleep` |
| 2842852 | `S,SN` | - | **transient** | `sleep` |
| 2843513 | `S,SN` | - | **transient** | `sleep` |
| 2843515 | `S,SN` | - | **transient** | `sleep` |
| 2843519 | `S,SN` | - | **transient** | `sleep` |
| 2844617 | `S,SN` | - | **transient** | `sleep` |
| 2844965 | `S,SN` | - | **transient** | `sleep` |
| 2845493 | `S,SN` | - | **transient** | `sleep` |
| 2845495 | `S,SN` | - | **transient** | `sleep` |
| 2845510 | `S,SN` | - | **transient** | `sleep` |
| 2846008 | `S,SN` | - | **transient** | `sleep` |
| 2846180 | `S,SN` | - | **transient** | `sleep` |
| 2846198 | `S,SN` | - | **transient** | `sleep` |
| 2846200 | `S,SN` | - | **transient** | `sleep` |
| 2846212 | `S,SN` | - | **transient** | `sleep` |
| 2846873 | `S,SN` | - | **transient** | `sleep` |
| 2846875 | `S,SN` | - | **transient** | `sleep` |
| 2846877 | `S,SN` | - | **transient** | `sleep` |
| 2846879 | `S,SN` | - | **transient** | `sleep` |
| 2847551 | `S,SN` | - | **transient** | `sleep` |
| 2847552 | `S,SN` | - | **transient** | `sleep` |
| 2847566 | `S,SN` | - | **transient** | `sleep` |
| 2848227 | `S,SN` | - | **transient** | `sleep` |
| 2848229 | `S,SN` | - | **transient** | `sleep` |
| 2848231 | `S,SN` | - | **transient** | `sleep` |
| 2848245 | `S,SN` | - | **transient** | `sleep` |
| 2848913 | `S,SN` | - | **transient** | `sleep` |
| 2848930 | `S,SN` | - | **transient** | `sleep` |
| 2848935 | `S,SN` | - | **transient** | `sleep` |
| 2848937 | `S,SN` | - | **transient** | `sleep` |
| 2849529 | `S,SN` | - | **transient** | `sleep` |
| 2849608 | `S,SN` | - | **transient** | `sleep` |
| 2849610 | `S,SN` | - | **transient** | `sleep` |
| 2849612 | `S,SN` | - | **transient** | `sleep` |
| 2849642 | `S,SN` | - | **transient** | `sleep` |
| 2850318 | `S` | - | **transient** | `systemd-userwor` |
| 2850321 | `S,SN` | - | **transient** | `sleep` |
| 2850322 | `S,SN` | - | **transient** | `sleep` |
| 2850324 | `S,SN` | - | **transient** | `sleep` |
| 2850337 | `S,SN` | - | **transient** | `sleep` |
| 2850999 | `S,SN` | - | **transient** | `sleep` |
| 2851001 | `S,SN` | - | **transient** | `sleep` |
| 2851011 | `S,SN` | - | **transient** | `sleep` |
| 2851031 | `S,SN` | - | **transient** | `sleep` |
| 2851066 | `S,SN` | - | **transient** | `sleep` |
| 2851692 | `S,SN` | - | **transient** | `sleep` |
| 2851694 | `S,SN` | - | **transient** | `sleep` |
| 2851722 | `S,SN` | - | **transient** | `sleep` |
| 2852365 | `S,SN` | - | **transient** | `sleep` |
| 2852367 | `S,SN` | - | **transient** | `sleep` |
| 2852369 | `S` | - | **transient** | `systemd-userwor` |
| 2852370 | `S` | - | **transient** | `systemd-userwor` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.27 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.26 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `PA1-perfA-r1.log` / `perfA-r1`  (13 snapshots)

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
| 50122 | `S,Sl` | 2.21 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.04 | both ends | `plasmashell` |
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
| 116643 | `S,Sl` | 0.12 | both ends | `kscreenlocker_g` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.79 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.14 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.15 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.35 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.11 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.35 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2735991 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2738649 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2738650 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2748015 | `S,SN` | - | **transient** | `sleep` |
| 2749332 | `S,SN` | - | **transient** | `sleep` |
| 2749344 | `S,SN` | - | **transient** | `sleep` |
| 2749348 | `S,SN` | - | **transient** | `sleep` |
| 2750022 | `S,SN` | - | **transient** | `sleep` |
| 2750024 | `S,SN` | - | **transient** | `sleep` |
| 2750026 | `S,SN` | - | **transient** | `sleep` |
| 2751328 | `S,SN` | - | **transient** | `sleep` |
| 2751337 | `S,SN` | - | **transient** | `sleep` |
| 2751339 | `S,SN` | - | **transient** | `sleep` |
| 2751340 | `S,SN` | - | **transient** | `sleep` |
| 2751342 | `S,SN` | - | **transient** | `sleep` |
| 2751344 | `S,SN` | - | **transient** | `sleep` |
| 2752022 | `S,SN` | - | **transient** | `sleep` |
| 2752024 | `S,SN` | - | **transient** | `sleep` |
| 2752027 | `S,SN` | - | **transient** | `sleep` |
| 2752030 | `S,SN` | - | **transient** | `sleep` |
| 2752700 | `S,SN` | - | **transient** | `sleep` |
| 2752702 | `S,SN` | - | **transient** | `sleep` |
| 2752704 | `S,SN` | - | **transient** | `sleep` |
| 2752798 | `S,SN` | - | **transient** | `sleep` |
| 2753172 | `S,SN` | - | **transient** | `sleep` |
| 2753359 | `S,SN` | - | **transient** | `sleep` |
| 2753363 | `S,SN` | - | **transient** | `sleep` |
| 2754019 | `S,SN` | - | **transient** | `sleep` |
| 2754020 | `S,SN` | - | **transient** | `sleep` |
| 2754022 | `S,SN` | - | **transient** | `sleep` |
| 2754024 | `S,SN` | - | **transient** | `sleep` |
| 2754682 | `S,SN` | - | **transient** | `sleep` |
| 2754684 | `S,SN` | - | **transient** | `sleep` |
| 2754687 | `S,SN` | - | **transient** | `sleep` |
| 2754689 | `S,SN` | - | **transient** | `sleep` |
| 2754761 | `S,SN` | - | **transient** | `sleep` |
| 2755358 | `S,SN` | - | **transient** | `sleep` |
| 2755361 | `S,SN` | - | **transient** | `sleep` |
| 2755733 | `S,SN` | - | **transient** | `sleep` |
| 2756018 | `S,SN` | - | **transient** | `sleep` |
| 2756020 | `S,SN` | - | **transient** | `sleep` |
| 2756676 | `S,SN` | - | **transient** | `sleep` |
| 2756679 | `S,SN` | - | **transient** | `sleep` |
| 2756680 | `S,SN` | - | **transient** | `sleep` |
| 2756707 | `S,SN` | - | **transient** | `sleep` |
| 2757335 | `S,SN` | - | **transient** | `sleep` |
| 2757337 | `S,SN` | - | **transient** | `sleep` |
| 2757340 | `S,SN` | - | **transient** | `sleep` |
| 2757997 | `S,SN` | - | **transient** | `sleep` |
| 2758000 | `S,SN` | - | **transient** | `sleep` |
| 2758030 | `S,SN` | - | **transient** | `sleep` |
| 2758679 | `S,SN` | - | **transient** | `sleep` |
| 2758681 | `S,SN` | - | **transient** | `sleep` |
| 2758683 | `S,SN` | - | **transient** | `sleep` |
| 2758685 | `S,SN` | - | **transient** | `sleep` |
| 2758687 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.21 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `PA2-perfA-r2.log` / `perfA-r2`  (13 snapshots)

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
| 1244 | `S,Ssl` | 0.01 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.01 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
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
| 50122 | `Rl,S,Sl` | 2.16 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
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
| 59463 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 1097257 | `S,Sl+` | 0.79 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.17 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.15 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.29 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.11 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.05 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.37 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2735991 | `S` | - | **transient** | `systemd-userwor` |
| 2738649 | `S` | - | **transient** | `systemd-userwor` |
| 2738650 | `S` | - | **transient** | `systemd-userwor` |
| 2756707 | `S,SN` | - | **transient** | `sleep` |
| 2757335 | `S,SN` | - | **transient** | `sleep` |
| 2757337 | `S,SN` | - | **transient** | `sleep` |
| 2757340 | `S,SN` | - | **transient** | `sleep` |
| 2757997 | `S,SN` | - | **transient** | `sleep` |
| 2758000 | `S,SN` | - | **transient** | `sleep` |
| 2758030 | `S,SN` | - | **transient** | `sleep` |
| 2758679 | `S,SN` | - | **transient** | `sleep` |
| 2758681 | `S,SN` | - | **transient** | `sleep` |
| 2758683 | `S,SN` | - | **transient** | `sleep` |
| 2758685 | `S,SN` | - | **transient** | `sleep` |
| 2758687 | `S,SN` | - | **transient** | `sleep` |
| 2759998 | `S,SN` | - | **transient** | `sleep` |
| 2760676 | `S,SN` | - | **transient** | `sleep` |
| 2760677 | `S,SN` | - | **transient** | `sleep` |
| 2760679 | `S,SN` | - | **transient** | `sleep` |
| 2760681 | `S,SN` | - | **transient** | `sleep` |
| 2761338 | `S,SN` | - | **transient** | `sleep` |
| 2761340 | `S,SN` | - | **transient** | `sleep` |
| 2761342 | `S,SN` | - | **transient** | `sleep` |
| 2761344 | `S,SN` | - | **transient** | `sleep` |
| 2761346 | `S,SN` | - | **transient** | `sleep` |
| 2762017 | `S,SN` | - | **transient** | `sleep` |
| 2762019 | `S,SN` | - | **transient** | `sleep` |
| 2762022 | `S,SN` | - | **transient** | `sleep` |
| 2762678 | `S,SN` | - | **transient** | `sleep` |
| 2762680 | `S,SN` | - | **transient** | `sleep` |
| 2763338 | `S,SN` | - | **transient** | `sleep` |
| 2763340 | `S,SN` | - | **transient** | `sleep` |
| 2763341 | `S,SN` | - | **transient** | `sleep` |
| 2763342 | `S` | - | **transient** | `systemd-userwor` |
| 2763344 | `S,SN` | - | **transient** | `sleep` |
| 2763632 | `S,SN` | - | **transient** | `sleep` |
| 2763999 | `S,SN` | - | **transient** | `sleep` |
| 2764001 | `S,SN` | - | **transient** | `sleep` |
| 2764004 | `S,SN` | - | **transient** | `sleep` |
| 2764660 | `S,SN` | - | **transient** | `sleep` |
| 2764677 | `S,SN` | - | **transient** | `sleep` |
| 2764693 | `S,SN` | - | **transient** | `sleep` |
| 2765333 | `S,SN` | - | **transient** | `sleep` |
| 2765336 | `S,SN` | - | **transient** | `sleep` |
| 2765338 | `S,SN` | - | **transient** | `sleep` |
| 2765340 | `S,SN` | - | **transient** | `sleep` |
| 2765997 | `S` | - | **transient** | `systemd-userwor` |
| 2765998 | `S` | - | **transient** | `systemd-userwor` |
| 2766000 | `S,SN` | - | **transient** | `sleep` |
| 2766002 | `S,SN` | - | **transient** | `sleep` |
| 2766006 | `S,SN` | - | **transient** | `sleep` |
| 2766659 | `S,SN` | - | **transient** | `sleep` |
| 2766661 | `S,SN` | - | **transient** | `sleep` |
| 2766666 | `S,SN` | - | **transient** | `sleep` |
| 2766668 | `S,SN` | - | **transient** | `sleep` |
| 2767340 | `S,SN` | - | **transient** | `sleep` |
| 2767350 | `S,SN` | - | **transient** | `sleep` |
| 2767353 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.23 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `PA3-perfA-r3.log` / `perfA-r3`  (13 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
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
| 50122 | `Rl,S,Sl` | 2.12 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
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
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.12 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.80 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.16 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.15 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.02 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.62 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.17 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.04 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.34 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2763342 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2765340 | `S,SN` | - | **transient** | `sleep` |
| 2765997 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2765998 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2766000 | `S,SN` | - | **transient** | `sleep` |
| 2766659 | `S,SN` | - | **transient** | `sleep` |
| 2766661 | `S,SN` | - | **transient** | `sleep` |
| 2766666 | `S,SN` | - | **transient** | `sleep` |
| 2766668 | `S,SN` | - | **transient** | `sleep` |
| 2767340 | `S,SN` | - | **transient** | `sleep` |
| 2767350 | `S,SN` | - | **transient** | `sleep` |
| 2767353 | `S,SN` | - | **transient** | `sleep` |
| 2768652 | `S,SN` | - | **transient** | `sleep` |
| 2768654 | `S,SN` | - | **transient** | `sleep` |
| 2768657 | `S,SN` | - | **transient** | `sleep` |
| 2768659 | `S,SN` | - | **transient** | `sleep` |
| 2769337 | `S,SN` | - | **transient** | `sleep` |
| 2769341 | `S,SN` | - | **transient** | `sleep` |
| 2769997 | `S,SN` | - | **transient** | `sleep` |
| 2769998 | `S,SN` | - | **transient** | `sleep` |
| 2770000 | `S,SN` | - | **transient** | `sleep` |
| 2770002 | `S,SN` | - | **transient** | `sleep` |
| 2770658 | `S,SN` | - | **transient** | `sleep` |
| 2770660 | `S,SN` | - | **transient** | `sleep` |
| 2770663 | `S,SN` | - | **transient** | `sleep` |
| 2770665 | `S,SN` | - | **transient** | `sleep` |
| 2771344 | `S,SN` | - | **transient** | `sleep` |
| 2771346 | `S,SN` | - | **transient** | `sleep` |
| 2771348 | `S,SN` | - | **transient** | `sleep` |
| 2772004 | `S,SN` | - | **transient** | `sleep` |
| 2772006 | `S,SN` | - | **transient** | `sleep` |
| 2772008 | `S,SN` | - | **transient** | `sleep` |
| 2772677 | `S,SN` | - | **transient** | `sleep` |
| 2772679 | `S,SN` | - | **transient** | `sleep` |
| 2772680 | `S,SN` | - | **transient** | `sleep` |
| 2772683 | `S,SN` | - | **transient** | `sleep` |
| 2773070 | `S,SN` | - | **transient** | `sleep` |
| 2773339 | `S,SN` | - | **transient** | `sleep` |
| 2773341 | `S,SN` | - | **transient** | `sleep` |
| 2773350 | `S,SN` | - | **transient** | `sleep` |
| 2773999 | `S,SN` | - | **transient** | `sleep` |
| 2774001 | `S,SN` | - | **transient** | `sleep` |
| 2774016 | `S,SN` | - | **transient** | `sleep` |
| 2774672 | `S,SN` | - | **transient** | `sleep` |
| 2774675 | `S,SN` | - | **transient** | `sleep` |
| 2774677 | `S,SN` | - | **transient** | `sleep` |
| 2774679 | `S,SN` | - | **transient** | `sleep` |
| 2775353 | `S,SN` | - | **transient** | `sleep` |
| 2775355 | `S,SN` | - | **transient** | `sleep` |
| 2775576 | `S,SN` | - | **transient** | `sleep` |
| 2776009 | `S,SN` | - | **transient** | `sleep` |
| 2776011 | `S,SN` | - | **transient** | `sleep` |
| 2776014 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.23 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `PB1-perfB-r1.log` / `perfB-r1`  (6 snapshots)

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
| 50122 | `S,Sl` | 0.81 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.26 | both ends | `claude` |
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
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.27 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2822271 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2822317 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2822318 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2822571 | `S,SN` | - | **transient** | `sleep` |
| 2822601 | `SN` | - | **transient** | `sleep` |
| 2822603 | `S,SN` | - | **transient** | `sleep` |
| 2822607 | `S,SN` | - | **transient** | `sleep` |
| 2822611 | `S,SN` | - | **transient** | `sleep` |
| 2822630 | `S,SN` | - | **transient** | `sleep` |
| 2822633 | `S,SN` | - | **transient** | `sleep` |
| 2822635 | `S,SN` | - | **transient** | `sleep` |
| 2822637 | `S,SN` | - | **transient** | `sleep` |
| 2822639 | `S,SN` | - | **transient** | `sleep` |
| 2822642 | `S,SN` | - | **transient** | `sleep` |
| 2822643 | `S,SN` | - | **transient** | `sleep` |
| 2822645 | `S,SN` | - | **transient** | `sleep` |
| 2823116 | `S,SN` | - | **transient** | `sleep` |
| 2823322 | `S,SN` | - | **transient** | `sleep` |
| 2823324 | `S,SN` | - | **transient** | `sleep` |
| 2823757 | `S,SN` | - | **transient** | `sleep` |
| 2823980 | `S,SN` | - | **transient** | `sleep` |
| 2824000 | `S,SN` | - | **transient** | `sleep` |
| 2824002 | `S,SN` | - | **transient** | `sleep` |
| 2824658 | `S,SN` | - | **transient** | `sleep` |
| 2824660 | `S,SN` | - | **transient** | `sleep` |
| 2824662 | `S,SN` | - | **transient** | `sleep` |
| 2824664 | `S,SN` | - | **transient** | `sleep` |
| 2824666 | `S,SN` | - | **transient** | `sleep` |
| 2825324 | `S,SN` | - | **transient** | `sleep` |
| 2825326 | `S,SN` | - | **transient** | `sleep` |
| 2825327 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.10 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `PB2-perfB-r2.log` / `perfB-r2`  (6 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.01 | both ends | `NetworkManager` |
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
| 1040 | `S,Ss` | 0.02 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.01 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.00 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.04 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.00 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
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
| 50122 | `S,Sl` | 0.98 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.02 | both ends | `plasmashell` |
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
| 1097257 | `S,Sl+` | 0.39 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.08 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.09 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `R,S,Sl` | 0.17 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2849642 | `S,SN` | - | **transient** | `sleep` |
| 2850318 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2851011 | `S,SN` | - | **transient** | `sleep` |
| 2851031 | `S,SN` | - | **transient** | `sleep` |
| 2851694 | `S,SN` | - | **transient** | `sleep` |
| 2851722 | `S,SN` | - | **transient** | `sleep` |
| 2852365 | `S,SN` | - | **transient** | `sleep` |
| 2852367 | `S,SN` | - | **transient** | `sleep` |
| 2852369 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2852370 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2853104 | `S,SN` | - | **transient** | `sleep` |
| 2853666 | `S,SN` | - | **transient** | `sleep` |
| 2853669 | `S,SN` | - | **transient** | `sleep` |
| 2853671 | `S,SN` | - | **transient** | `sleep` |
| 2853673 | `S,SN` | - | **transient** | `sleep` |
| 2853678 | `S,SN` | - | **transient** | `sleep` |
| 2854357 | `S,SN` | - | **transient** | `sleep` |
| 2854366 | `S,SN` | - | **transient** | `sleep` |
| 2854378 | `S,SN` | - | **transient** | `sleep` |
| 2854379 | `S,SN` | - | **transient** | `sleep` |
| 2855035 | `S,SN` | - | **transient** | `sleep` |
| 2855037 | `S,SN` | - | **transient** | `sleep` |
| 2855039 | `S,SN` | - | **transient** | `sleep` |
| 2855041 | `S,SN` | - | **transient** | `sleep` |
| 2855044 | `S,SN` | - | **transient** | `sleep` |
| 2855048 | `Sl` | - | **transient** | `tailscaled` |
| 2855055 | `Sl` | - | **transient** | `fish` |
| 2855082 | `R` | - | **transient** | `conda` |
| 2855734 | `S,Ssl` | - | **transient** | `tailscaled` |
| 2855741 | `S` | - | **transient** | `fish` |
| 2855838 | `S,Ss` | - | **transient** | `systemd-hostnam` |
| 2855867 | `S,SN` | - | **transient** | `sleep` |
| 2855907 | `S,S+` | - | **transient** | `tmux:` |
| 2855910 | `S,SN` | - | **transient** | `sleep` |
| 2855911 | `S,SN` | - | **transient** | `sleep` |
| 2855914 | `S,SN` | - | **transient** | `sleep` |
| 2855917 | `S,SN` | - | **transient** | `sleep` |
| 2856586 | `S,SN` | - | **transient** | `sleep` |
| 2856588 | `S,SN` | - | **transient** | `sleep` |
| 2856599 | `S,SN` | - | **transient** | `sleep` |
| 2856614 | `RN` | - | **transient** | `pgrep` |
| 2856624 | `S,SN` | - | **transient** | `sleep` |
| 2856797 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.09 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `PB3-perfB-r3.log` / `perfB-r3`  (6 snapshots)

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
| 1244 | `S,Ssl` | 0.04 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 0.85 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.29 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.07 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.07 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.27 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.03 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2850318 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2852369 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2852370 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2855035 | `S,SN` | - | **transient** | `sleep` |
| 2855039 | `S,SN` | - | **transient** | `sleep` |
| 2855044 | `S,SN` | - | **transient** | `sleep` |
| 2855734 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
| 2855741 | `S` | 0.00 | both ends | `fish` |
| 2855838 | `S,Ss` | - | **transient** | `systemd-hostnam` |
| 2855867 | `S,SN` | 0.00 | both ends | `sleep` |
| 2855907 | `S,S+` | 0.00 | both ends | `tmux:` |
| 2855910 | `S,SN` | - | **transient** | `sleep` |
| 2855911 | `S,SN` | - | **transient** | `sleep` |
| 2855914 | `S,SN` | - | **transient** | `sleep` |
| 2855917 | `S,SN` | - | **transient** | `sleep` |
| 2856586 | `S,SN` | - | **transient** | `sleep` |
| 2856588 | `S,SN` | - | **transient** | `sleep` |
| 2856599 | `S,SN` | - | **transient** | `sleep` |
| 2856624 | `S,SN` | - | **transient** | `sleep` |
| 2856797 | `S,SN` | - | **transient** | `sleep` |
| 2858599 | `S,SN` | - | **transient** | `sleep` |
| 2858601 | `S,SN` | - | **transient** | `sleep` |
| 2858781 | `S,SN` | - | **transient** | `sleep` |
| 2859268 | `S,SN` | - | **transient** | `sleep` |
| 2859270 | `S,SN` | - | **transient** | `sleep` |
| 2859939 | `S,SN` | - | **transient** | `sleep` |
| 2859940 | `S,SN` | - | **transient** | `sleep` |
| 2859942 | `S,SN` | - | **transient** | `sleep` |
| 2859945 | `S,SN` | - | **transient** | `sleep` |
| 2860083 | `S,SN` | - | **transient** | `sleep` |
| 2860609 | `S,SN` | - | **transient** | `sleep` |
| 2860612 | `S,SN` | - | **transient** | `sleep` |
| 2860621 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.09 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W1-wall-r1.log` / `wall-r1`  (13 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.01 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
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
| 1965 | `S,S<sl` | 0.01 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.03 | both ends | `wireplumber` |
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
| 4094 | `S,Ss` | 0.02 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 2.25 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.12 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.83 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.17 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.16 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.61 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.18 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.04 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.37 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2735991 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2738649 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2738650 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2741978 | `SN` | - | **transient** | `sleep` |
| 2742620 | `S,SN` | - | **transient** | `sleep` |
| 2742626 | `S,SN` | - | **transient** | `sleep` |
| 2742628 | `S,SN` | - | **transient** | `sleep` |
| 2742651 | `S,SN` | - | **transient** | `sleep` |
| 2742653 | `S,SN` | - | **transient** | `sleep` |
| 2742658 | `S,SN` | - | **transient** | `sleep` |
| 2742660 | `S,SN` | - | **transient** | `sleep` |
| 2742663 | `S,SN` | - | **transient** | `sleep` |
| 2742671 | `S,SN` | - | **transient** | `sleep` |
| 2742673 | `S,SN` | - | **transient** | `sleep` |
| 2742674 | `S,SN` | - | **transient** | `sleep` |
| 2742676 | `S,SN` | - | **transient** | `sleep` |
| 2742790 | `S,SN` | - | **transient** | `sleep` |
| 2743354 | `S,SN` | - | **transient** | `sleep` |
| 2743356 | `S,SN` | - | **transient** | `sleep` |
| 2743358 | `S,SN` | - | **transient** | `sleep` |
| 2743667 | `S,SN` | - | **transient** | `sleep` |
| 2744012 | `S,SN` | - | **transient** | `sleep` |
| 2744031 | `S,SN` | - | **transient** | `sleep` |
| 2744686 | `S,SN` | - | **transient** | `sleep` |
| 2744688 | `S,SN` | - | **transient** | `sleep` |
| 2744690 | `S,SN` | - | **transient** | `sleep` |
| 2744692 | `S,SN` | - | **transient** | `sleep` |
| 2745347 | `S,SN` | - | **transient** | `sleep` |
| 2745349 | `S,SN` | - | **transient** | `sleep` |
| 2745510 | `S,SN` | - | **transient** | `sleep` |
| 2746002 | `S,SN` | - | **transient** | `sleep` |
| 2746005 | `S,SN` | - | **transient** | `sleep` |
| 2746007 | `S,SN` | - | **transient** | `sleep` |
| 2746154 | `S,SN` | - | **transient** | `sleep` |
| 2746665 | `S,SN` | - | **transient** | `sleep` |
| 2746668 | `S,SN` | - | **transient** | `sleep` |
| 2747346 | `S,SN` | - | **transient** | `sleep` |
| 2747348 | `S,SN` | - | **transient** | `sleep` |
| 2747350 | `S,SN` | - | **transient** | `sleep` |
| 2747352 | `S,SN` | - | **transient** | `sleep` |
| 2747354 | `S,SN` | - | **transient** | `sleep` |
| 2747356 | `S,SN` | - | **transient** | `sleep` |
| 2748015 | `S,SN` | - | **transient** | `sleep` |
| 2748017 | `S,SN` | - | **transient** | `sleep` |
| 2748672 | `S,SN` | - | **transient** | `sleep` |
| 2748674 | `S,SN` | - | **transient** | `sleep` |
| 2748676 | `S,SN` | - | **transient** | `sleep` |
| 2749332 | `S,SN` | - | **transient** | `sleep` |
| 2749334 | `S,SN` | - | **transient** | `sleep` |
| 2749344 | `S,SN` | - | **transient** | `sleep` |
| 2749346 | `S,SN` | - | **transient** | `sleep` |
| 2749348 | `S,SN` | - | **transient** | `sleep` |
| 2750019 | `S,SN` | - | **transient** | `sleep` |
| 2750022 | `S,SN` | - | **transient** | `sleep` |
| 2750024 | `S,SN` | - | **transient** | `sleep` |
| 2750026 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.24 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W2-wall-r2.log` / `wall-r2`  (13 snapshots)

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
| 977 | `S,Ssl` | 0.03 | both ends | `NetworkManager` |
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
| 1218 | `S,Ss` | 0.02 | both ends | `wpa_supplicant` |
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
| 4094 | `S,Ss` | 0.02 | both ends | `tmux:` |
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
| 50122 | `R,S,Sl` | 2.38 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `Rsl,S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.05 | both ends | `plasmashell` |
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
| 59463 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 116643 | `S,Sl` | 0.13 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.99 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.18 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.13 | both ends | `codex` |
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
| 2707653 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2707657 | `S,SN` | - | **transient** | `sleep` |
| 2707659 | `S,SN` | - | **transient** | `sleep` |
| 2707661 | `S,SN` | - | **transient** | `sleep` |
| 2707665 | `SN` | - | **transient** | `sleep` |
| 2707668 | `S,SN` | - | **transient** | `sleep` |
| 2707671 | `S,SN` | - | **transient** | `sleep` |
| 2707688 | `S,SN` | - | **transient** | `sleep` |
| 2707689 | `S,SN` | - | **transient** | `sleep` |
| 2707695 | `S,SN` | - | **transient** | `sleep` |
| 2707698 | `S,SN` | - | **transient** | `sleep` |
| 2707699 | `S,SN` | - | **transient** | `sleep` |
| 2707701 | `S,SN` | - | **transient** | `sleep` |
| 2707711 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2707712 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2707714 | `S,SN` | - | **transient** | `sleep` |
| 2708107 | `S,SN` | - | **transient** | `sleep` |
| 2708389 | `S,SN` | - | **transient** | `sleep` |
| 2708391 | `S,SN` | - | **transient** | `sleep` |
| 2708393 | `S,SN` | - | **transient** | `sleep` |
| 2709050 | `S,SN` | - | **transient** | `sleep` |
| 2709052 | `S,SN` | - | **transient** | `sleep` |
| 2709054 | `S,SN` | - | **transient** | `sleep` |
| 2709056 | `S,SN` | - | **transient** | `sleep` |
| 2709058 | `S,SN` | - | **transient** | `sleep` |
| 2709730 | `S,SN` | - | **transient** | `sleep` |
| 2709732 | `S,SN` | - | **transient** | `sleep` |
| 2709734 | `S,SN` | - | **transient** | `sleep` |
| 2710389 | `S,SN` | - | **transient** | `sleep` |
| 2710391 | `S,SN` | - | **transient** | `sleep` |
| 2711046 | `S,SN` | - | **transient** | `sleep` |
| 2711049 | `S,SN` | - | **transient** | `sleep` |
| 2711050 | `S,SN` | - | **transient** | `sleep` |
| 2711052 | `S,SN` | - | **transient** | `sleep` |
| 2711700 | `S,SN` | - | **transient** | `sleep` |
| 2711702 | `S,SN` | - | **transient** | `sleep` |
| 2711714 | `S,SN` | - | **transient** | `sleep` |
| 2711717 | `S,SN` | - | **transient** | `sleep` |
| 2711720 | `S,SN` | - | **transient** | `sleep` |
| 2712390 | `S,SN` | - | **transient** | `sleep` |
| 2712392 | `S,SN` | - | **transient** | `sleep` |
| 2712395 | `S,SN` | - | **transient** | `sleep` |
| 2712397 | `S,SN` | - | **transient** | `sleep` |
| 2712399 | `S,SN` | - | **transient** | `sleep` |
| 2712414 | `S,SN` | - | **transient** | `sleep` |
| 2713054 | `S,SN` | - | **transient** | `sleep` |
| 2713056 | `S,SN` | - | **transient** | `sleep` |
| 2713710 | `S,SN` | - | **transient** | `sleep` |
| 2713712 | `S,SN` | - | **transient** | `sleep` |
| 2713714 | `S,SN` | - | **transient** | `sleep` |
| 2714372 | `S,SN` | - | **transient** | `sleep` |
| 2714374 | `S,SN` | - | **transient** | `sleep` |
| 2714384 | `S,SN` | - | **transient** | `sleep` |
| 2714388 | `S,SN` | - | **transient** | `sleep` |
| 2714390 | `S,SN` | - | **transient** | `sleep` |
| 2715079 | `S,SN` | - | **transient** | `sleep` |
| 2715082 | `S,SN` | - | **transient** | `sleep` |
| 2715083 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.53 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.11 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.04 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.41 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.19 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W3-wall-r3.log` / `wall-r3`  (13 snapshots)

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
| 979 | `S,Ssl` | 0.01 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `D,Ds,S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.04 | both ends | `udisksd` |
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
| 50122 | `Rl,S,Sl` | 2.10 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.04 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.03 | both ends | `plasmashell` |
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
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.12 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.85 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.16 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.14 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2707653 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2707711 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2707712 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2713054 | `S,SN` | - | **transient** | `sleep` |
| 2713712 | `S,SN` | - | **transient** | `sleep` |
| 2713714 | `S,SN` | - | **transient** | `sleep` |
| 2714372 | `S,SN` | - | **transient** | `sleep` |
| 2714374 | `S,SN` | - | **transient** | `sleep` |
| 2714384 | `S,SN` | - | **transient** | `sleep` |
| 2714388 | `S,SN` | - | **transient** | `sleep` |
| 2714390 | `S,SN` | - | **transient** | `sleep` |
| 2715079 | `S,SN` | - | **transient** | `sleep` |
| 2715082 | `S,SN` | - | **transient** | `sleep` |
| 2715083 | `S,SN` | - | **transient** | `sleep` |
| 2715924 | `S,SN` | - | **transient** | `sleep` |
| 2716367 | `S,SN` | - | **transient** | `sleep` |
| 2717044 | `S,SN` | - | **transient** | `sleep` |
| 2717046 | `S,SN` | - | **transient** | `sleep` |
| 2717165 | `S,SN` | - | **transient** | `sleep` |
| 2717699 | `S,SN` | - | **transient** | `sleep` |
| 2717701 | `S,SN` | - | **transient** | `sleep` |
| 2717703 | `S,SN` | - | **transient** | `sleep` |
| 2717705 | `S,SN` | - | **transient** | `sleep` |
| 2718361 | `S,SN` | - | **transient** | `sleep` |
| 2718364 | `S,SN` | - | **transient** | `sleep` |
| 2719033 | `S,SN` | - | **transient** | `sleep` |
| 2719035 | `S,SN` | - | **transient** | `sleep` |
| 2719037 | `S,SN` | - | **transient** | `sleep` |
| 2719040 | `S,SN` | - | **transient** | `sleep` |
| 2719042 | `S,SN` | - | **transient** | `sleep` |
| 2719044 | `S,SN` | - | **transient** | `sleep` |
| 2719701 | `S,SN` | - | **transient** | `sleep` |
| 2719703 | `S,SN` | - | **transient** | `sleep` |
| 2720357 | `S,SN` | - | **transient** | `sleep` |
| 2720359 | `S,SN` | - | **transient** | `sleep` |
| 2720361 | `S,SN` | - | **transient** | `sleep` |
| 2720669 | `S,SN` | - | **transient** | `sleep` |
| 2720780 | `S,SN` | - | **transient** | `sleep` |
| 2721016 | `S,SN` | - | **transient** | `sleep` |
| 2721018 | `S,SN` | - | **transient** | `sleep` |
| 2721020 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.29 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.07 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2721799 | `S,SN` | - | **transient** | `sleep` |
| 2721802 | `S,SN` | - | **transient** | `sleep` |
| 2721804 | `S,SN` | - | **transient** | `sleep` |
| 2721806 | `S,SN` | - | **transient** | `sleep` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.35 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722653 | `S,SN` | - | **transient** | `sleep` |
| 2722656 | `S,SN` | - | **transient** | `sleep` |
| 2722912 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2723359 | `S,SN` | - | **transient** | `sleep` |
| 2723360 | `S,SN` | - | **transient** | `sleep` |
| 2723362 | `S,SN` | - | **transient** | `sleep` |
| 2723364 | `S,SN` | - | **transient** | `sleep` |
| 2724019 | `S,SN` | - | **transient** | `sleep` |
| 2724022 | `S,SN` | - | **transient** | `sleep` |
| 2724027 | `S,SN` | - | **transient** | `sleep` |
| 2724029 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.23 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W4-wall-r4.log` / `wall-r4`  (13 snapshots)

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
| 50122 | `S,Sl` | 2.41 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
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
| 59466 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.13 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.82 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.15 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.14 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2707653 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2707711 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2707712 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2719701 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.30 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.10 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2721802 | `S,SN` | - | **transient** | `sleep` |
| 2721806 | `S,SN` | - | **transient** | `sleep` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.06 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `R,S,Sl` | 0.37 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722653 | `S,SN` | - | **transient** | `sleep` |
| 2722656 | `S,SN` | - | **transient** | `sleep` |
| 2722912 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2723359 | `S,SN` | - | **transient** | `sleep` |
| 2723360 | `SN` | - | **transient** | `sleep` |
| 2723362 | `S,SN` | - | **transient** | `sleep` |
| 2723364 | `S,SN` | - | **transient** | `sleep` |
| 2724019 | `S,SN` | - | **transient** | `sleep` |
| 2724022 | `S,SN` | - | **transient** | `sleep` |
| 2724027 | `S,SN` | - | **transient** | `sleep` |
| 2724029 | `S,SN` | - | **transient** | `sleep` |
| 2725950 | `S,SN` | - | **transient** | `sleep` |
| 2726008 | `S,SN` | - | **transient** | `sleep` |
| 2726010 | `S,SN` | - | **transient** | `sleep` |
| 2726013 | `S,SN` | - | **transient** | `sleep` |
| 2726015 | `S,SN` | - | **transient** | `sleep` |
| 2726017 | `S,SN` | - | **transient** | `sleep` |
| 2726673 | `S,SN` | - | **transient** | `sleep` |
| 2726675 | `S,SN` | - | **transient** | `sleep` |
| 2727329 | `S,SN` | - | **transient** | `sleep` |
| 2727331 | `S,SN` | - | **transient** | `sleep` |
| 2727333 | `S,SN` | - | **transient** | `sleep` |
| 2727336 | `S,SN` | - | **transient** | `sleep` |
| 2727338 | `S,SN` | - | **transient** | `sleep` |
| 2727993 | `S,SN` | - | **transient** | `sleep` |
| 2727995 | `S,SN` | - | **transient** | `sleep` |
| 2727997 | `S,SN` | - | **transient** | `sleep` |
| 2728670 | `S,SN` | - | **transient** | `sleep` |
| 2728672 | `S,SN` | - | **transient** | `sleep` |
| 2728675 | `S,SN` | - | **transient** | `sleep` |
| 2728677 | `S,SN` | - | **transient** | `sleep` |
| 2728857 | `S,SN` | - | **transient** | `sleep` |
| 2729324 | `S,SN` | - | **transient** | `sleep` |
| 2729336 | `S,SN` | - | **transient** | `sleep` |
| 2729337 | `S,SN` | - | **transient** | `sleep` |
| 2729339 | `S,SN` | - | **transient** | `sleep` |
| 2729341 | `S,SN` | - | **transient** | `sleep` |
| 2729996 | `S,SN` | - | **transient** | `sleep` |
| 2729998 | `S,SN` | - | **transient** | `sleep` |
| 2730002 | `S,SN` | - | **transient** | `sleep` |
| 2730657 | `S,SN` | - | **transient** | `sleep` |
| 2730673 | `S,SN` | - | **transient** | `sleep` |
| 2730675 | `S,SN` | - | **transient** | `sleep` |
| 2731210 | `S,SN` | - | **transient** | `sleep` |
| 2731346 | `S,SN` | - | **transient** | `sleep` |
| 2731348 | `S,SN` | - | **transient** | `sleep` |
| 2731350 | `S,SN` | - | **transient** | `sleep` |
| 2732007 | `S,SN` | - | **transient** | `sleep` |
| 2732009 | `S,SN` | - | **transient** | `sleep` |
| 2732136 | `S,SN` | - | **transient** | `sleep` |
| 2732664 | `S,SN` | - | **transient** | `sleep` |
| 2732666 | `S,SN` | - | **transient** | `sleep` |
| 2732671 | `S,SN` | - | **transient** | `sleep` |
| 2732672 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.25 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W5-wall-r5.log` / `wall-r5`  (13 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.00 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.02 | both ends | `irqbalance` |
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
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
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
| 2082 | `S` | 0.01 | both ends | `dbus-broker` |
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
| 4094 | `S,Ss` | 0.02 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 2.28 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.01 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.04 | both ends | `plasmashell` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.87 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.15 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.18 | both ends | `codex` |
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
| 2707653 | `S` | - | **transient** | `systemd-userwor` |
| 2707711 | `S` | - | **transient** | `systemd-userwor` |
| 2707712 | `S` | - | **transient** | `systemd-userwor` |
| 2721602 | `S,Ssl` | 0.54 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.06 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `Rl,S,Sl` | 0.13 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `R,S,Sl` | 0.10 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.36 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2731210 | `S,SN` | - | **transient** | `sleep` |
| 2731346 | `S,SN` | - | **transient** | `sleep` |
| 2731350 | `S,SN` | - | **transient** | `sleep` |
| 2732007 | `S,SN` | - | **transient** | `sleep` |
| 2732009 | `S,SN` | - | **transient** | `sleep` |
| 2732136 | `S,SN` | - | **transient** | `sleep` |
| 2732664 | `S,SN` | - | **transient** | `sleep` |
| 2732666 | `S,SN` | - | **transient** | `sleep` |
| 2732671 | `S,SN` | - | **transient** | `sleep` |
| 2732672 | `S,SN` | - | **transient** | `sleep` |
| 2733965 | `S,SN` | - | **transient** | `sleep` |
| 2733968 | `S,SN` | - | **transient** | `sleep` |
| 2733970 | `S,SN` | - | **transient** | `sleep` |
| 2734664 | `S,SN` | - | **transient** | `sleep` |
| 2734666 | `S,SN` | - | **transient** | `sleep` |
| 2734668 | `S,SN` | - | **transient** | `sleep` |
| 2734670 | `S,SN` | - | **transient** | `sleep` |
| 2735325 | `S,SN` | - | **transient** | `sleep` |
| 2735335 | `S,SN` | - | **transient** | `sleep` |
| 2735990 | `S,SN` | - | **transient** | `sleep` |
| 2735991 | `S` | - | **transient** | `systemd-userwor` |
| 2735992 | `S,SN` | - | **transient** | `sleep` |
| 2735995 | `S,SN` | - | **transient** | `sleep` |
| 2735997 | `S,SN` | - | **transient** | `sleep` |
| 2736652 | `S,SN` | - | **transient** | `sleep` |
| 2736654 | `S,SN` | - | **transient** | `sleep` |
| 2736658 | `S,SN` | - | **transient** | `sleep` |
| 2736660 | `S,SN` | - | **transient** | `sleep` |
| 2737329 | `S,SN` | - | **transient** | `sleep` |
| 2737331 | `S,SN` | - | **transient** | `sleep` |
| 2737333 | `S,SN` | - | **transient** | `sleep` |
| 2737988 | `S,SN` | - | **transient** | `sleep` |
| 2737990 | `S,SN` | - | **transient** | `sleep` |
| 2737994 | `S,SN` | - | **transient** | `sleep` |
| 2738649 | `S` | - | **transient** | `systemd-userwor` |
| 2738650 | `S` | - | **transient** | `systemd-userwor` |
| 2738652 | `S,SN` | - | **transient** | `sleep` |
| 2738654 | `S,SN` | - | **transient** | `sleep` |
| 2738655 | `S,SN` | - | **transient** | `sleep` |
| 2738657 | `S,SN` | - | **transient** | `sleep` |
| 2739328 | `S,SN` | - | **transient** | `sleep` |
| 2739333 | `S,SN` | - | **transient** | `sleep` |
| 2739334 | `S,SN` | - | **transient** | `sleep` |
| 2739989 | `S,SN` | - | **transient** | `sleep` |
| 2739991 | `S,SN` | - | **transient** | `sleep` |
| 2739993 | `S,SN` | - | **transient** | `sleep` |
| 2740164 | `S,SN` | - | **transient** | `sleep` |
| 2740665 | `S,SN` | - | **transient** | `sleep` |
| 2740667 | `S,SN` | - | **transient** | `sleep` |
| 2740670 | `S,SN` | - | **transient** | `sleep` |
| 2740672 | `S,SN` | - | **transient** | `sleep` |
| 2741327 | `S,SN` | - | **transient** | `sleep` |
| 2741478 | `S,SN` | - | **transient** | `sleep` |
| 2741974 | `S,SN` | - | **transient** | `sleep` |
| 2741976 | `S,SN` | - | **transient** | `sleep` |
| 2741978 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.24 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |
