# Every foreign pid of every batch, with every state observed for it

Settler ruling R13 (W5 T8.9r fix round 3): R2' asks for every foreign pid and state seen in any snapshot, listed -- not a count and a busiest-five. This is that list, written by `scripts/idle_proof.py` from the same snapshots the fractions are computed from.

States are the UNION of `/proc/<pid>/stat`'s single character (`FOREIGN_TICK`) and `ps`'s full string (`FOREIGN_PS`). A pid marked **transient** was present in some snapshot of the batch but not in both ends, so it has no CPU delta. `delta` is ticks/clk across the batch window for pids present at both ends.

## `A1-alloc-r1.log` / `alloc-r1`  (6 snapshots)

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
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 0.74 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.01 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.01 | both ends | `mysqld` |
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
| 1097257 | `S,Sl+` | 0.32 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
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
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
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
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.06 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3040544 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040546 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040547 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3043895 | `S,SN` | - | **transient** | `sleep` |
| 3045230 | `S,SN` | - | **transient** | `sleep` |
| 3045235 | `S,SN` | - | **transient** | `sleep` |
| 3045237 | `S,SN` | - | **transient** | `sleep` |
| 3045242 | `S,SN` | - | **transient** | `sleep` |
| 3045244 | `S,SN` | - | **transient** | `sleep` |
| 3045246 | `S,SN` | - | **transient** | `sleep` |
| 3045248 | `S,SN` | - | **transient** | `sleep` |
| 3045250 | `S,SN` | - | **transient** | `sleep` |
| 3045254 | `S,SN` | - | **transient** | `sleep` |
| 3045256 | `S,SN` | - | **transient** | `sleep` |
| 3045258 | `S,SN` | - | **transient** | `sleep` |
| 3045260 | `S,SN` | - | **transient** | `sleep` |
| 3045264 | `S,SN` | - | **transient** | `sleep` |
| 3045959 | `S,SN` | - | **transient** | `sleep` |
| 3045960 | `S,SN` | - | **transient** | `sleep` |
| 3045962 | `S,SN` | - | **transient** | `sleep` |
| 3045964 | `S,SN` | - | **transient** | `sleep` |
| 3045966 | `S,SN` | - | **transient** | `sleep` |
| 3046629 | `S,SN` | - | **transient** | `sleep` |
| 3046631 | `S,SN` | - | **transient** | `sleep` |
| 3046633 | `S,SN` | - | **transient** | `sleep` |
| 3046635 | `S,SN` | - | **transient** | `sleep` |
| 3047298 | `S,SN` | - | **transient** | `sleep` |
| 3047307 | `S,SN` | - | **transient** | `sleep` |
| 3047974 | `S,SN` | - | **transient** | `sleep` |
| 3047976 | `S,SN` | - | **transient** | `sleep` |
| 3047978 | `S,SN` | - | **transient** | `sleep` |
| 3047981 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.08 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `A2-alloc-r2.log` / `alloc-r2`  (6 snapshots)

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
| 993 | `S,SNsl` | 0.01 | both ends | `rtkit-daemon` |
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
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.35 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.01 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.19 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.01 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.03 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3040544 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040546 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040547 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3045959 | `S,SN` | - | **transient** | `sleep` |
| 3045962 | `S,SN` | - | **transient** | `sleep` |
| 3045966 | `S,SN` | - | **transient** | `sleep` |
| 3046629 | `S,SN` | - | **transient** | `sleep` |
| 3046631 | `S,SN` | - | **transient** | `sleep` |
| 3046633 | `S,SN` | 0.00 | both ends | `sleep` |
| 3046635 | `S,SN` | - | **transient** | `sleep` |
| 3047298 | `S,SN` | - | **transient** | `sleep` |
| 3047307 | `S,SN` | - | **transient** | `sleep` |
| 3047974 | `S,SN` | - | **transient** | `sleep` |
| 3047976 | `S,SN` | - | **transient** | `sleep` |
| 3047978 | `S,SN` | - | **transient** | `sleep` |
| 3047981 | `S,SN` | 0.00 | both ends | `sleep` |
| 3049133 | `S,SN` | 0.00 | both ends | `sleep` |
| 3049973 | `S,SN` | - | **transient** | `sleep` |
| 3049974 | `S,SN` | - | **transient** | `sleep` |
| 3049976 | `S,SN` | - | **transient** | `sleep` |
| 3049978 | `S,SN` | - | **transient** | `sleep` |
| 3049980 | `S,SN` | - | **transient** | `sleep` |
| 3049982 | `S,SN` | - | **transient** | `sleep` |
| 3050644 | `S,SN` | - | **transient** | `sleep` |
| 3051306 | `S,SN` | - | **transient** | `sleep` |
| 3051308 | `S,SN` | - | **transient** | `sleep` |
| 3051309 | `S,SN` | - | **transient** | `sleep` |
| 3051979 | `S,SN` | - | **transient** | `sleep` |
| 3051981 | `S,SN` | - | **transient** | `sleep` |
| 3051983 | `S,SN` | - | **transient** | `sleep` |
| 3051985 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.07 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `A3-alloc-r3.log` / `alloc-r3`  (6 snapshots)

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
| 50122 | `S,Sl` | 0.78 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.31 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.08 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.07 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 3067441 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3067443 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3067444 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3068044 | `S,SN` | - | **transient** | `sleep` |
| 3068046 | `S,SN` | - | **transient** | `sleep` |
| 3068048 | `S,SN` | - | **transient** | `sleep` |
| 3068052 | `S,SN` | - | **transient** | `sleep` |
| 3068067 | `S,SN` | - | **transient** | `sleep` |
| 3068069 | `S,SN` | - | **transient** | `sleep` |
| 3068071 | `S,SN` | - | **transient** | `sleep` |
| 3068073 | `S,SN` | - | **transient** | `sleep` |
| 3068075 | `S,SN` | - | **transient** | `sleep` |
| 3068089 | `S,SN` | 0.00 | both ends | `sleep` |
| 3068091 | `S,SN` | - | **transient** | `sleep` |
| 3068093 | `S,SN` | - | **transient** | `sleep` |
| 3068095 | `S,SN` | - | **transient** | `sleep` |
| 3068766 | `S,SN` | - | **transient** | `sleep` |
| 3068767 | `S,SN` | - | **transient** | `sleep` |
| 3069423 | `S,SN` | - | **transient** | `sleep` |
| 3069424 | `S,SN` | - | **transient** | `sleep` |
| 3069426 | `S,SN` | - | **transient** | `sleep` |
| 3070080 | `S,SN` | - | **transient** | `sleep` |
| 3070098 | `S,SN` | - | **transient** | `sleep` |
| 3070100 | `S,SN` | - | **transient** | `sleep` |
| 3070102 | `S,SN` | - | **transient** | `sleep` |
| 3070104 | `S,SN` | - | **transient** | `sleep` |
| 3070106 | `S,SN` | - | **transient** | `sleep` |
| 3070732 | `S,SN` | - | **transient** | `sleep` |
| 3070767 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.08 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `A4-alloc-r4.log` / `alloc-r4`  (6 snapshots)

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
| 50122 | `R,S,Sl` | 0.96 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.00 | both ends | `plasmashell` |
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
| 1097257 | `S,Sl+` | 0.37 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.14 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.39 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.01 | both ends | `codex-code-mode` |
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
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.01 | both ends | `node-22` |
| 3040544 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040546 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040547 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3053737 | `S,SN` | - | **transient** | `sleep` |
| 3054022 | `S,SN` | - | **transient** | `sleep` |
| 3054025 | `S,SN` | - | **transient** | `sleep` |
| 3054029 | `S,SN` | - | **transient** | `sleep` |
| 3054748 | `S,SN` | - | **transient** | `sleep` |
| 3054977 | `S,SN` | - | **transient** | `sleep` |
| 3055201 | `S,SN` | - | **transient** | `sleep` |
| 3055408 | `S,SN` | 0.00 | both ends | `sleep` |
| 3055410 | `S,SN` | - | **transient** | `sleep` |
| 3055414 | `S,SN` | - | **transient** | `sleep` |
| 3055415 | `S,SN` | - | **transient** | `sleep` |
| 3056082 | `S,SN` | - | **transient** | `sleep` |
| 3056084 | `S,SN` | - | **transient** | `sleep` |
| 3056152 | `S,SN` | - | **transient** | `sleep` |
| 3058139 | `S,SN` | - | **transient** | `sleep` |
| 3058156 | `S,SN` | - | **transient** | `sleep` |
| 3058157 | `S,SN` | - | **transient** | `sleep` |
| 3058159 | `S,SN` | - | **transient** | `sleep` |
| 3058161 | `S,SN` | - | **transient** | `sleep` |
| 3058174 | `S,SN` | - | **transient** | `sleep` |
| 3058176 | `S,SN` | - | **transient** | `sleep` |
| 3058846 | `S,SN` | - | **transient** | `sleep` |
| 3058854 | `S,SN` | - | **transient** | `sleep` |
| 3058901 | `S,SN` | - | **transient** | `sleep` |
| 3060210 | `S,SN` | - | **transient** | `sleep` |
| 3060212 | `S,SN` | - | **transient** | `sleep` |
| 3060874 | `S,SN` | - | **transient** | `sleep` |
| 3060914 | `S,SN` | - | **transient** | `sleep` |
| 3060916 | `S,SN` | - | **transient** | `sleep` |
| 3060919 | `S,SN` | - | **transient** | `sleep` |
| 3060923 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.09 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `A5-alloc-r5.log` / `alloc-r5`  (6 snapshots)

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
| 977 | `S,Ssl` | 0.02 | both ends | `NetworkManager` |
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
| 50122 | `S,Sl` | 0.83 | both ends | `kwin_wayland` |
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
| 583906 | `S,SNs` | 0.01 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.34 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
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
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3040544 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040546 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040547 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3055408 | `S,SN` | - | **transient** | `sleep` |
| 3058156 | `S,SN` | - | **transient** | `sleep` |
| 3058161 | `S,SN` | - | **transient** | `sleep` |
| 3058174 | `S,SN` | - | **transient** | `sleep` |
| 3058846 | `S,SN` | - | **transient** | `sleep` |
| 3058854 | `S,SN` | - | **transient** | `sleep` |
| 3058901 | `S,SN` | - | **transient** | `sleep` |
| 3060210 | `S,SN` | - | **transient** | `sleep` |
| 3060212 | `S,SN` | - | **transient** | `sleep` |
| 3060874 | `S,SN` | - | **transient** | `sleep` |
| 3060914 | `S,SN` | - | **transient** | `sleep` |
| 3060916 | `S,SN` | - | **transient** | `sleep` |
| 3060919 | `S,SN` | 0.00 | both ends | `sleep` |
| 3060923 | `S,SN` | - | **transient** | `sleep` |
| 3062913 | `S,SN` | - | **transient** | `sleep` |
| 3062914 | `S,SN` | - | **transient** | `sleep` |
| 3062916 | `S,SN` | - | **transient** | `sleep` |
| 3062918 | `S,SN` | - | **transient** | `sleep` |
| 3062920 | `S,SN` | - | **transient** | `sleep` |
| 3063583 | `S,SN` | - | **transient** | `sleep` |
| 3063585 | `S,SN` | - | **transient** | `sleep` |
| 3063587 | `S,SN` | - | **transient** | `sleep` |
| 3063590 | `S,SN` | - | **transient** | `sleep` |
| 3063833 | `S,SN` | - | **transient** | `sleep` |
| 3064255 | `S,SN` | - | **transient** | `sleep` |
| 3064923 | `S,SN` | - | **transient** | `sleep` |
| 3064925 | `S,SN` | - | **transient** | `sleep` |
| 3064927 | `S,SN` | - | **transient** | `sleep` |
| 3064933 | `RN` | - | **transient** | `pgrep` |
| 3064944 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.07 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `F1-wallpf-r1.log` / `wallpf-r1`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.39 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.03 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.18 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.01 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `Rl,S,Sl` | 0.06 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 3067441 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3067443 | `S` | - | **transient** | `systemd-userwor` |
| 3067444 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3068089 | `S,SN` | 0.00 | both ends | `sleep` |
| 3069423 | `SN` | - | **transient** | `sleep` |
| 3069424 | `S,SN` | - | **transient** | `sleep` |
| 3069426 | `S,SN` | - | **transient** | `sleep` |
| 3070080 | `S,SN` | 0.00 | both ends | `sleep` |
| 3070098 | `S,SN` | - | **transient** | `sleep` |
| 3070100 | `S,SN` | 0.00 | both ends | `sleep` |
| 3070102 | `S,SN` | - | **transient** | `sleep` |
| 3070104 | `S,SN` | 0.00 | both ends | `sleep` |
| 3070106 | `S,SN` | 0.00 | both ends | `sleep` |
| 3070732 | `S,SN` | - | **transient** | `sleep` |
| 3070767 | `S,SN` | 0.00 | both ends | `sleep` |
| 3071837 | `S,SN` | 0.00 | both ends | `sleep` |
| 3072446 | `S,SN` | - | **transient** | `sleep` |
| 3072712 | `S,SN` | - | **transient** | `sleep` |
| 3073373 | `S,SN` | - | **transient** | `sleep` |
| 3073375 | `S,SN` | - | **transient** | `sleep` |
| 3073377 | `S,SN` | - | **transient** | `sleep` |
| 3073378 | `S` | - | **transient** | `systemd-userwor` |
| 3073380 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.05 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `F2-wallpf-r2.log` / `wallpf-r2`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.40 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.00 | both ends | `plasmashell` |
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
| 116643 | `S,Sl` | 0.02 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.16 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.04 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.01 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.00 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 3067441 | `S` | - | **transient** | `systemd-userwor` |
| 3067444 | `S` | - | **transient** | `systemd-userwor` |
| 3068089 | `S,SN` | - | **transient** | `sleep` |
| 3070080 | `SN` | - | **transient** | `sleep` |
| 3070100 | `S,SN` | - | **transient** | `sleep` |
| 3070104 | `S,SN` | - | **transient** | `sleep` |
| 3070106 | `S,SN` | - | **transient** | `sleep` |
| 3070767 | `S,SN` | - | **transient** | `sleep` |
| 3071837 | `S,SN` | 0.00 | both ends | `sleep` |
| 3072446 | `S,SN` | - | **transient** | `sleep` |
| 3072712 | `S,SN` | - | **transient** | `sleep` |
| 3073373 | `S,SN` | - | **transient** | `sleep` |
| 3073375 | `S,SN` | 0.00 | both ends | `sleep` |
| 3073377 | `S,SN` | 0.00 | both ends | `sleep` |
| 3073378 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3073380 | `S,SN` | 0.00 | both ends | `sleep` |
| 3074747 | `S,SN` | - | **transient** | `sleep` |
| 3075342 | `S,SN` | - | **transient** | `sleep` |
| 3075344 | `S,SN` | - | **transient** | `sleep` |
| 3075345 | `S` | - | **transient** | `systemd-userwor` |
| 3075347 | `S,SN` | - | **transient** | `sleep` |
| 3075349 | `S,SN` | - | **transient** | `sleep` |
| 3075351 | `S,SN` | - | **transient** | `sleep` |
| 3075949 | `S` | - | **transient** | `systemd-userwor` |
| 3076006 | `S,SN` | - | **transient** | `sleep` |
| 3076008 | `S,SN` | - | **transient** | `sleep` |
| 3076217 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.03 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `F3-wallpf-r3.log` / `wallpf-r3`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.40 | both ends | `kwin_wayland` |
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
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.02 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.17 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.01 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.03 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 3073375 | `S,SN` | - | **transient** | `sleep` |
| 3073377 | `S,SN` | - | **transient** | `sleep` |
| 3073378 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3073380 | `S,SN` | - | **transient** | `sleep` |
| 3074747 | `S,SN` | 0.00 | both ends | `sleep` |
| 3075342 | `S,SN` | - | **transient** | `sleep` |
| 3075344 | `S,SN` | 0.00 | both ends | `sleep` |
| 3075345 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3075347 | `S,SN` | 0.00 | both ends | `sleep` |
| 3075349 | `S,SN` | 0.00 | both ends | `sleep` |
| 3075351 | `S,SN` | - | **transient** | `sleep` |
| 3075949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3076006 | `S,SN` | 0.00 | both ends | `sleep` |
| 3076008 | `S,SN` | 0.00 | both ends | `sleep` |
| 3076217 | `S,SN` | 0.00 | both ends | `sleep` |
| 3077276 | `S,SN` | 0.00 | both ends | `sleep` |
| 3077959 | `S,SN` | - | **transient** | `sleep` |
| 3077960 | `S,SN` | - | **transient** | `sleep` |
| 3078616 | `S,SN` | - | **transient** | `sleep` |
| 3078618 | `S,SN` | - | **transient** | `sleep` |
| 3078620 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.04 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `F4-wallpf-r4.log` / `wallpf-r4`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.40 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.02 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.16 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.04 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.04 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.25 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 3073378 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3074747 | `SN` | - | **transient** | `sleep` |
| 3075344 | `S,SN` | - | **transient** | `sleep` |
| 3075345 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3075347 | `S,SN` | - | **transient** | `sleep` |
| 3075349 | `S,SN` | - | **transient** | `sleep` |
| 3075949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3076006 | `S,SN` | 0.00 | both ends | `sleep` |
| 3076008 | `S,SN` | - | **transient** | `sleep` |
| 3076217 | `S,SN` | - | **transient** | `sleep` |
| 3077276 | `S,SN` | 0.00 | both ends | `sleep` |
| 3077959 | `S,SN` | - | **transient** | `sleep` |
| 3077960 | `S,SN` | - | **transient** | `sleep` |
| 3078616 | `S,SN` | 0.00 | both ends | `sleep` |
| 3078618 | `S,SN` | 0.00 | both ends | `sleep` |
| 3078620 | `S,SN` | 0.00 | both ends | `sleep` |
| 3080034 | `S,SN` | - | **transient** | `sleep` |
| 3080584 | `S,SN` | - | **transient** | `sleep` |
| 3080587 | `S,SN` | - | **transient** | `sleep` |
| 3080589 | `S,SN` | - | **transient** | `sleep` |
| 3080591 | `S,SN` | - | **transient** | `sleep` |
| 3080593 | `S,SN` | - | **transient** | `sleep` |
| 3081127 | `S,SN` | - | **transient** | `sleep` |
| 3081248 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.05 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `F5-wallpf-r5.log` / `wallpf-r5`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.41 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.03 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.15 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.01 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.00 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3035219 | `S,SN` | - | **transient** | `sleep` |
| 3038254 | `S,SN` | - | **transient** | `sleep` |
| 3038545 | `S,SN` | - | **transient** | `sleep` |
| 3038547 | `S,SN` | - | **transient** | `sleep` |
| 3039858 | `S,SN` | - | **transient** | `sleep` |
| 3039860 | `S,SN` | - | **transient** | `sleep` |
| 3039862 | `S,SN` | - | **transient** | `sleep` |
| 3040543 | `S,SN` | - | **transient** | `sleep` |
| 3040544 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040546 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040547 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3040548 | `S,SN` | - | **transient** | `sleep` |
| 3041210 | `S,SN` | 0.00 | both ends | `sleep` |
| 3041212 | `S,SN` | 0.00 | both ends | `sleep` |
| 3041214 | `S,SN` | - | **transient** | `sleep` |
| 3041217 | `S,SN` | 0.00 | both ends | `sleep` |
| 3041227 | `S,SN` | 0.00 | both ends | `sleep` |
| 3043219 | `S,SN` | - | **transient** | `sleep` |
| 3043220 | `S,SN` | - | **transient** | `sleep` |
| 3043222 | `S,SN` | - | **transient** | `sleep` |
| 3043224 | `S,SN` | - | **transient** | `sleep` |
| 3043226 | `S,SN` | - | **transient** | `sleep` |
| 3043891 | `S,SN` | - | **transient** | `sleep` |
| 3043893 | `S,SN` | - | **transient** | `sleep` |
| 3043895 | `S,SN` | - | **transient** | `sleep` |
| 3043897 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.04 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `M1-mem-r1.log` / `mem-r1`  (5 snapshots)

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
| 50122 | `S,Sl` | 0.62 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.27 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.07 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.16 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
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
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2987948 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987950 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3009571 | `S,SN` | - | **transient** | `sleep` |
| 3009573 | `S,SN` | - | **transient** | `sleep` |
| 3009595 | `S,SN` | - | **transient** | `sleep` |
| 3009612 | `S,SN` | - | **transient** | `sleep` |
| 3009616 | `S,SN` | - | **transient** | `sleep` |
| 3009618 | `S,SN` | - | **transient** | `sleep` |
| 3009620 | `S,SN` | - | **transient** | `sleep` |
| 3009623 | `S,SN` | 0.00 | both ends | `sleep` |
| 3009625 | `S,SN` | - | **transient** | `sleep` |
| 3009627 | `S,SN` | - | **transient** | `sleep` |
| 3009629 | `S,SN` | 0.00 | both ends | `sleep` |
| 3009631 | `S,SN` | 0.00 | both ends | `sleep` |
| 3009634 | `S,SN` | - | **transient** | `sleep` |
| 3009636 | `S,SN` | - | **transient** | `sleep` |
| 3010319 | `S,SN` | - | **transient** | `sleep` |
| 3010322 | `S,SN` | - | **transient** | `sleep` |
| 3010325 | `S,SN` | - | **transient** | `sleep` |
| 3010986 | `S,SN` | - | **transient** | `sleep` |
| 3011021 | `S,SN` | - | **transient** | `sleep` |
| 3011023 | `S,SN` | - | **transient** | `sleep` |
| 3011027 | `S,SN` | - | **transient** | `sleep` |
| 3011032 | `S,SN` | - | **transient** | `sleep` |
| 3011039 | `S,SN` | - | **transient** | `sleep` |
| 3011403 | `S,SN` | - | **transient** | `sleep` |
| 3011706 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.06 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `M2-mem-r2.log` / `mem-r2`  (5 snapshots)

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
| 50122 | `S,Sl` | 0.70 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.00 | both ends | `plasmashell` |
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
| 1097257 | `S,Sl+` | 0.21 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.05 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.21 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.01 | both ends | `codex-code-mode` |
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
| 2721602 | `S,Ssl` | 0.03 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2987948 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987950 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2991336 | `S,SNsl` | 0.04 | both ends | `node-22` |
| 3009623 | `S,SN` | - | **transient** | `sleep` |
| 3009629 | `S,SN` | 0.00 | both ends | `sleep` |
| 3009631 | `SN` | - | **transient** | `sleep` |
| 3010319 | `SN` | - | **transient** | `sleep` |
| 3010322 | `S,SN` | - | **transient** | `sleep` |
| 3010325 | `S,SN` | - | **transient** | `sleep` |
| 3010986 | `S,SN` | - | **transient** | `sleep` |
| 3011021 | `S,SN` | - | **transient** | `sleep` |
| 3011023 | `S,SN` | - | **transient** | `sleep` |
| 3011027 | `S,SN` | - | **transient** | `sleep` |
| 3011032 | `S,SN` | - | **transient** | `sleep` |
| 3011039 | `S,SN` | - | **transient** | `sleep` |
| 3011403 | `S,SN` | - | **transient** | `sleep` |
| 3011706 | `S,SN` | 0.00 | both ends | `sleep` |
| 3013190 | `S,SN` | - | **transient** | `sleep` |
| 3013342 | `S,SN` | - | **transient** | `sleep` |
| 3013680 | `S,SN` | - | **transient** | `sleep` |
| 3014341 | `S,SN` | - | **transient** | `sleep` |
| 3014343 | `S,SN` | - | **transient** | `sleep` |
| 3014345 | `S,SN` | - | **transient** | `sleep` |
| 3014347 | `S,SN` | - | **transient** | `sleep` |
| 3014350 | `S,SN` | - | **transient** | `sleep` |
| 3015012 | `S,SN` | - | **transient** | `sleep` |
| 3015083 | `S,SN` | - | **transient** | `sleep` |
| 3015084 | `S,SN` | - | **transient** | `sleep` |
| 3015086 | `S,SN` | - | **transient** | `sleep` |
| 3015089 | `S,SN` | - | **transient** | `sleep` |
| 3015091 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.06 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `M3-mem-r3.log` / `mem-r3`  (5 snapshots)

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
| 50103 | `S,Ssl` | 0.01 | both ends | `seapplet` |
| 50111 | `S,Ssl` | 0.00 | both ends | `kwin_wayland_wr` |
| 50121 | `S,Ssl` | 0.00 | both ends | `DiscoverNotifie` |
| 50122 | `S,Sl` | 0.62 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
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
| 116643 | `S,Sl` | 0.02 | both ends | `kscreenlocker_g` |
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
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.10 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.25 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2987948 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987950 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3009629 | `S,SN` | - | **transient** | `sleep` |
| 3011706 | `S,SN` | - | **transient** | `sleep` |
| 3013342 | `S,SN` | - | **transient** | `sleep` |
| 3014341 | `S,SN` | - | **transient** | `sleep` |
| 3014343 | `S,SN` | - | **transient** | `sleep` |
| 3014345 | `S,SN` | - | **transient** | `sleep` |
| 3014347 | `S,SN` | - | **transient** | `sleep` |
| 3014350 | `S,SN` | 0.00 | both ends | `sleep` |
| 3015012 | `S,SN` | 0.00 | both ends | `sleep` |
| 3015083 | `S,SN` | 0.00 | both ends | `sleep` |
| 3015084 | `S,SN` | - | **transient** | `sleep` |
| 3015086 | `S,SN` | 0.00 | both ends | `sleep` |
| 3015089 | `S,SN` | - | **transient** | `sleep` |
| 3015091 | `S,SN` | 0.00 | both ends | `sleep` |
| 3017068 | `S,SN` | - | **transient** | `sleep` |
| 3017070 | `S,SN` | - | **transient** | `sleep` |
| 3017072 | `S,SN` | - | **transient** | `sleep` |
| 3017556 | `S,SN` | - | **transient** | `sleep` |
| 3017732 | `S,SN` | - | **transient** | `sleep` |
| 3017734 | `S,SN` | - | **transient** | `sleep` |
| 3018397 | `S,SN` | - | **transient** | `sleep` |
| 3018400 | `S,SN` | - | **transient** | `sleep` |
| 3018403 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.06 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `M4-mem-r4.log` / `mem-r4`  (5 snapshots)

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
| 50122 | `S,Sl` | 0.61 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.01 | both ends | `akonadiserver` |
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
| 116643 | `S,Sl` | 0.03 | both ends | `kscreenlocker_g` |
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
| 1861779 | `S,Sl` | 0.10 | both ends | `codex` |
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
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3019769 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019770 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019771 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3020670 | `S,SN` | - | **transient** | `sleep` |
| 3020794 | `SN` | - | **transient** | `sleep` |
| 3020799 | `S,SN` | - | **transient** | `sleep` |
| 3020815 | `S,SN` | - | **transient** | `sleep` |
| 3021486 | `S,SN` | - | **transient** | `sleep` |
| 3021489 | `S,SN` | - | **transient** | `sleep` |
| 3021490 | `S,SN` | - | **transient** | `sleep` |
| 3021516 | `S,SN` | - | **transient** | `sleep` |
| 3021518 | `S,SN` | - | **transient** | `sleep` |
| 3021541 | `S,SN` | 0.00 | both ends | `sleep` |
| 3021543 | `S,SN` | 0.00 | both ends | `sleep` |
| 3021545 | `S,SN` | - | **transient** | `sleep` |
| 3021548 | `S,SN` | 0.00 | both ends | `sleep` |
| 3021552 | `S,SN` | 0.00 | both ends | `sleep` |
| 3021940 | `S,SN` | - | **transient** | `sleep` |
| 3022241 | `S,SN` | - | **transient** | `sleep` |
| 3022243 | `S,SN` | - | **transient** | `sleep` |
| 3022245 | `S,SN` | - | **transient** | `sleep` |
| 3022247 | `S,SN` | - | **transient** | `sleep` |
| 3022910 | `S,SN` | - | **transient** | `sleep` |
| 3022911 | `S,SN` | - | **transient** | `sleep` |
| 3022913 | `S,SN` | - | **transient** | `sleep` |
| 3022915 | `S,SN` | - | **transient** | `sleep` |
| 3022918 | `S,SN` | - | **transient** | `sleep` |
| 3023577 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.05 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `M5-mem-r5.log` / `mem-r5`  (5 snapshots)

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
| 50122 | `S,Sl` | 0.62 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.27 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.05 | both ends | `codex` |
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
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.02 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.00 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.00 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3019769 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019770 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019771 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3021541 | `SN` | - | **transient** | `sleep` |
| 3021543 | `S,SN` | - | **transient** | `sleep` |
| 3021548 | `S,SN` | - | **transient** | `sleep` |
| 3021552 | `S,SN` | - | **transient** | `sleep` |
| 3021940 | `S,SN` | - | **transient** | `sleep` |
| 3022243 | `S,SN` | - | **transient** | `sleep` |
| 3022247 | `S,SN` | - | **transient** | `sleep` |
| 3022910 | `S,SN` | - | **transient** | `sleep` |
| 3022911 | `S,SN` | - | **transient** | `sleep` |
| 3022913 | `S,SN` | 0.00 | both ends | `sleep` |
| 3022915 | `S,SN` | - | **transient** | `sleep` |
| 3022918 | `S,SN` | - | **transient** | `sleep` |
| 3023577 | `S,SN` | - | **transient** | `sleep` |
| 3024369 | `S,SN` | - | **transient** | `sleep` |
| 3024976 | `S,SN` | - | **transient** | `sleep` |
| 3025554 | `S,SN` | - | **transient** | `sleep` |
| 3025558 | `S,SN` | - | **transient** | `sleep` |
| 3025791 | `S,SN` | - | **transient** | `sleep` |
| 3026075 | `S,SN` | - | **transient** | `sleep` |
| 3026250 | `S,SN` | - | **transient** | `sleep` |
| 3026252 | `S,SN` | - | **transient** | `sleep` |
| 3026254 | `S,SN` | - | **transient** | `sleep` |
| 3026256 | `S,SN` | - | **transient** | `sleep` |
| 3026918 | `S,SN` | - | **transient** | `sleep` |
| 3026920 | `S,SN` | - | **transient** | `sleep` |
| 3026923 | `S,SN` | - | **transient** | `sleep` |
| 3026924 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.06 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `M6-mem-r6.log` / `mem-r6`  (5 snapshots)

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
| 50122 | `S,Sl` | 0.72 | both ends | `kwin_wayland` |
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
| 50651 | `S,Ssl` | 0.01 | both ends | `xdg-desktop-por` |
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
| 116643 | `S,Sl` | 0.03 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.24 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
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
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.01 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.01 | both ends | `node-22` |
| 3019769 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019770 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019771 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3022913 | `S,SN` | 0.00 | both ends | `sleep` |
| 3024976 | `S,SN` | - | **transient** | `sleep` |
| 3025554 | `S,SN` | - | **transient** | `sleep` |
| 3025558 | `S,SN` | - | **transient** | `sleep` |
| 3025791 | `S,SN` | - | **transient** | `sleep` |
| 3026075 | `S,SN` | - | **transient** | `sleep` |
| 3026250 | `SN` | - | **transient** | `sleep` |
| 3026252 | `S,SN` | - | **transient** | `sleep` |
| 3026254 | `S,SN` | - | **transient** | `sleep` |
| 3026256 | `S,SN` | - | **transient** | `sleep` |
| 3026918 | `S,SN` | 0.00 | both ends | `sleep` |
| 3026920 | `S,SN` | 0.00 | both ends | `sleep` |
| 3026923 | `S,SN` | 0.00 | both ends | `sleep` |
| 3026924 | `S,SN` | - | **transient** | `sleep` |
| 3028797 | `S,SN` | - | **transient** | `sleep` |
| 3028897 | `S,SN` | - | **transient** | `sleep` |
| 3029105 | `S,SN` | - | **transient** | `sleep` |
| 3029556 | `S,SN` | - | **transient** | `sleep` |
| 3029558 | `S,SN` | - | **transient** | `sleep` |
| 3029562 | `S,SN` | - | **transient** | `sleep` |
| 3030236 | `S,SN` | - | **transient** | `sleep` |
| 3030237 | `S,SN` | - | **transient** | `sleep` |
| 3030239 | `S,SN` | - | **transient** | `sleep` |
| 3030241 | `S,SN` | - | **transient** | `sleep` |
| 3030243 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.06 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P1-perf.log` / `perf`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.36 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.03 | both ends | `kded6` |
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
| 116643 | `S,Sl` | 0.02 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.10 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.03 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.00 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2894658 | `S,SN` | 0.00 | both ends | `sleep` |
| 2894673 | `S,SN` | - | **transient** | `sleep` |
| 2894684 | `S,SN` | - | **transient** | `sleep` |
| 2894693 | `S,SN` | - | **transient** | `sleep` |
| 2894697 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894698 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894699 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894731 | `S,SN` | - | **transient** | `sleep` |
| 2897323 | `S,SN` | 0.00 | both ends | `sleep` |
| 2897325 | `S,SN` | - | **transient** | `sleep` |
| 2897328 | `S,SN` | 0.00 | both ends | `sleep` |
| 2897333 | `S,SN` | - | **transient** | `sleep` |
| 2898571 | `S,SN` | 0.00 | both ends | `sleep` |
| 2899958 | `S,SN` | 0.00 | both ends | `sleep` |
| 2899960 | `S,SN` | 0.00 | both ends | `sleep` |
| 2899962 | `S,SN` | 0.00 | both ends | `sleep` |
| 2900638 | `S,SN` | - | **transient** | `sleep` |
| 2900649 | `S,SN` | - | **transient** | `sleep` |
| 2900658 | `S,SN` | - | **transient** | `sleep` |
| 2900660 | `S,SN` | - | **transient** | `sleep` |
| 2901316 | `S,SN` | - | **transient** | `sleep` |
| 2901318 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.04 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P2-perfstat-r1.log` / `perfstat-r1`  (8 snapshots)

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
| 50122 | `S,Sl` | 1.18 | both ends | `kwin_wayland` |
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
| 59466 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.06 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.45 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.11 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.09 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
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
| 2721602 | `S,Ssl` | 0.34 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.09 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2894697 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894698 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894699 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2902632 | `S,SN` | - | **transient** | `sleep` |
| 2902654 | `S,SN` | - | **transient** | `sleep` |
| 2902666 | `S,SN` | - | **transient** | `sleep` |
| 2902675 | `S,SN` | - | **transient** | `sleep` |
| 2902680 | `S,SN` | - | **transient** | `sleep` |
| 2902682 | `S,SN` | - | **transient** | `sleep` |
| 2902684 | `S,SN` | - | **transient** | `sleep` |
| 2902686 | `S,SN` | - | **transient** | `sleep` |
| 2902688 | `S,SN` | - | **transient** | `sleep` |
| 2902690 | `S,SN` | - | **transient** | `sleep` |
| 2902692 | `S,SN` | - | **transient** | `sleep` |
| 2902694 | `S,SN` | - | **transient** | `sleep` |
| 2902697 | `S,SN` | - | **transient** | `sleep` |
| 2903373 | `S,SN` | - | **transient** | `sleep` |
| 2903389 | `S,SN` | - | **transient** | `sleep` |
| 2903398 | `S,SN` | - | **transient** | `sleep` |
| 2903400 | `S,SN` | - | **transient** | `sleep` |
| 2904005 | `S,SN` | - | **transient** | `sleep` |
| 2904060 | `S,SN` | - | **transient** | `sleep` |
| 2904457 | `S,SN` | - | **transient** | `sleep` |
| 2904719 | `S,SN` | - | **transient** | `sleep` |
| 2904721 | `S,SN` | - | **transient** | `sleep` |
| 2904723 | `S,SN` | - | **transient** | `sleep` |
| 2904725 | `S,SN` | - | **transient** | `sleep` |
| 2905386 | `S,SN` | - | **transient** | `sleep` |
| 2905388 | `S,SN` | - | **transient** | `sleep` |
| 2905390 | `S,SN` | - | **transient** | `sleep` |
| 2905909 | `S,SN` | - | **transient** | `sleep` |
| 2906048 | `S,SN` | - | **transient** | `sleep` |
| 2906052 | `S,SN` | - | **transient** | `sleep` |
| 2906158 | `S,SN` | - | **transient** | `sleep` |
| 2906724 | `S,SN` | - | **transient** | `sleep` |
| 2906725 | `S,SN` | - | **transient** | `sleep` |
| 2906727 | `S,SN` | - | **transient** | `sleep` |
| 2906739 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.14 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P2-perfstat-r2.log` / `perfstat-r2`  (8 snapshots)

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
| 50122 | `S,Sl` | 1.19 | both ends | `kwin_wayland` |
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
| 50658 | `S,Sl` | 0.01 | both ends | `mysqld` |
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
| 116643 | `S,Sl` | 0.06 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.47 | both ends | `claude` |
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
| 2721602 | `S,Ssl` | 0.27 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.03 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.17 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2894697 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894698 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894699 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2904719 | `S,SN` | - | **transient** | `sleep` |
| 2904721 | `S,SN` | - | **transient** | `sleep` |
| 2904725 | `S,SN` | - | **transient** | `sleep` |
| 2905388 | `S,SN` | - | **transient** | `sleep` |
| 2905390 | `S,SN` | - | **transient** | `sleep` |
| 2905909 | `S,SN` | - | **transient** | `sleep` |
| 2906048 | `S,SN` | - | **transient** | `sleep` |
| 2906052 | `S,SN` | - | **transient** | `sleep` |
| 2906158 | `S,SN` | - | **transient** | `sleep` |
| 2906724 | `S,SN` | - | **transient** | `sleep` |
| 2906725 | `S,SN` | - | **transient** | `sleep` |
| 2906727 | `S,SN` | - | **transient** | `sleep` |
| 2906739 | `S,SN` | - | **transient** | `sleep` |
| 2908708 | `S,SN` | - | **transient** | `sleep` |
| 2908710 | `S,SN` | - | **transient** | `sleep` |
| 2908712 | `S,SN` | - | **transient** | `sleep` |
| 2909371 | `S,SN` | - | **transient** | `sleep` |
| 2909373 | `S,SN` | - | **transient** | `sleep` |
| 2909816 | `S,SN` | - | **transient** | `sleep` |
| 2910034 | `S,SN` | - | **transient** | `sleep` |
| 2910038 | `S,SN` | - | **transient** | `sleep` |
| 2910041 | `S,SN` | - | **transient** | `sleep` |
| 2910709 | `S,SN` | - | **transient** | `sleep` |
| 2910718 | `S,SN` | - | **transient** | `sleep` |
| 2910720 | `S,SN` | - | **transient** | `sleep` |
| 2910721 | `S,SN` | - | **transient** | `sleep` |
| 2910723 | `S,SN` | - | **transient** | `sleep` |
| 2911382 | `S,SN` | - | **transient** | `sleep` |
| 2911384 | `S,SN` | - | **transient** | `sleep` |
| 2911387 | `S,SN` | - | **transient** | `sleep` |
| 2911389 | `S,SN` | - | **transient** | `sleep` |
| 2911391 | `S,SN` | - | **transient** | `sleep` |
| 2912053 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.18 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.04 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P2-perfstat-r3.log` / `perfstat-r3`  (8 snapshots)

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
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 1.35 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.07 | both ends | `kscreenlocker_g` |
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
| 1097257 | `R,S,Sl+` | 0.48 | both ends | `claude` |
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
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.13 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.13 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2894697 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894698 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2894699 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2909816 | `S,SN` | - | **transient** | `sleep` |
| 2910038 | `S,SN` | - | **transient** | `sleep` |
| 2910041 | `S,SN` | - | **transient** | `sleep` |
| 2910709 | `S,SN` | - | **transient** | `sleep` |
| 2910720 | `S,SN` | - | **transient** | `sleep` |
| 2910723 | `S,SN` | - | **transient** | `sleep` |
| 2911382 | `S,SN` | - | **transient** | `sleep` |
| 2911384 | `S,SN` | - | **transient** | `sleep` |
| 2911387 | `S,SN` | - | **transient** | `sleep` |
| 2911389 | `S,SN` | - | **transient** | `sleep` |
| 2911391 | `S,SN` | 0.00 | both ends | `sleep` |
| 2912053 | `S,SN` | - | **transient** | `sleep` |
| 2913129 | `S,SN` | - | **transient** | `sleep` |
| 2914023 | `S,SN` | - | **transient** | `sleep` |
| 2914025 | `S,SN` | - | **transient** | `sleep` |
| 2914027 | `S,SN` | - | **transient** | `sleep` |
| 2914687 | `S,SN` | - | **transient** | `sleep` |
| 2914689 | `S,SN` | - | **transient** | `sleep` |
| 2914691 | `S,SN` | - | **transient** | `sleep` |
| 2914692 | `S,SN` | - | **transient** | `sleep` |
| 2914694 | `S,SN` | - | **transient** | `sleep` |
| 2914696 | `S,SN` | - | **transient** | `sleep` |
| 2915356 | `S,SN` | - | **transient** | `sleep` |
| 2915358 | `S,SN` | - | **transient** | `sleep` |
| 2915360 | `S,SN` | - | **transient** | `sleep` |
| 2916020 | `S,SN` | - | **transient** | `sleep` |
| 2916494 | `S,SN` | - | **transient** | `sleep` |
| 2916673 | `S,SN` | - | **transient** | `sleep` |
| 2916675 | `S,SN` | - | **transient** | `sleep` |
| 2916677 | `S,SN` | - | **transient** | `sleep` |
| 2916688 | `S,SN` | - | **transient** | `sleep` |
| 2916696 | `S,SN` | - | **transient** | `sleep` |
| 2916699 | `S,SN` | - | **transient** | `sleep` |
| 2917371 | `S,SN` | - | **transient** | `sleep` |
| 2917373 | `S,SN` | - | **transient** | `sleep` |
| 2917375 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.14 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P3-perfrec-r1.log` / `perfrec-r1`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.40 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.00 | both ends | `plasmashell` |
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
| 116643 | `S,Sl` | 0.02 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.17 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.03 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.00 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2960014 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960016 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960017 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2976189 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976209 | `S,SN` | - | **transient** | `sleep` |
| 2976212 | `S,SN` | - | **transient** | `sleep` |
| 2976215 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976217 | `S,SN` | - | **transient** | `sleep` |
| 2976226 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976235 | `S,SN` | - | **transient** | `sleep` |
| 2976236 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976238 | `S,SN` | - | **transient** | `sleep` |
| 2976240 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976242 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976244 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976246 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976920 | `S,SN` | - | **transient** | `sleep` |
| 2977577 | `S,SN` | - | **transient** | `sleep` |
| 2977579 | `S,SN` | - | **transient** | `sleep` |
| 2977581 | `S,SN` | - | **transient** | `sleep` |
| 2977583 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.03 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P3-perfrec-r2.log` / `perfrec-r2`  (4 snapshots)

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
| 50122 | `Rl,S,Sl` | 0.39 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.02 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.13 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.04 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.00 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2960014 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960016 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960017 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2976189 | `S,SN` | 0.00 | both ends | `sleep` |
| 2976215 | `SN` | - | **transient** | `sleep` |
| 2976226 | `S,SN` | - | **transient** | `sleep` |
| 2976236 | `S,SN` | - | **transient** | `sleep` |
| 2976240 | `S,SN` | - | **transient** | `sleep` |
| 2976242 | `S,SN` | - | **transient** | `sleep` |
| 2976244 | `S,SN` | - | **transient** | `sleep` |
| 2976246 | `S,SN` | - | **transient** | `sleep` |
| 2976920 | `S,SN` | - | **transient** | `sleep` |
| 2977577 | `S,SN` | - | **transient** | `sleep` |
| 2977579 | `S,SN` | 0.00 | both ends | `sleep` |
| 2977581 | `S,SN` | 0.00 | both ends | `sleep` |
| 2977583 | `S,SN` | 0.00 | both ends | `sleep` |
| 2978943 | `S,SN` | - | **transient** | `sleep` |
| 2979565 | `S,SN` | - | **transient** | `sleep` |
| 2979575 | `S,SN` | - | **transient** | `sleep` |
| 2979576 | `S,SN` | - | **transient** | `sleep` |
| 2979578 | `S,SN` | - | **transient** | `sleep` |
| 2979580 | `S,SN` | - | **transient** | `sleep` |
| 2980238 | `S,SN` | - | **transient** | `sleep` |
| 2980240 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.05 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `P3-perfrec-r3.log` / `perfrec-r3`  (4 snapshots)

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
| 50122 | `S,Sl` | 0.40 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.03 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.11 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.03 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.03 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2960014 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960016 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960017 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2977579 | `S,SN` | - | **transient** | `sleep` |
| 2977581 | `S,SN` | - | **transient** | `sleep` |
| 2977583 | `S,SN` | - | **transient** | `sleep` |
| 2978943 | `S,SN` | 0.00 | both ends | `sleep` |
| 2979565 | `S,SN` | 0.00 | both ends | `sleep` |
| 2979575 | `S,SN` | - | **transient** | `sleep` |
| 2979576 | `S,SN` | - | **transient** | `sleep` |
| 2979578 | `S,SN` | 0.00 | both ends | `sleep` |
| 2979580 | `S,SN` | 0.00 | both ends | `sleep` |
| 2980238 | `S,SN` | 0.00 | both ends | `sleep` |
| 2980240 | `S,SN` | 0.00 | both ends | `sleep` |
| 2981394 | `S,SN` | 0.00 | both ends | `sleep` |
| 2981539 | `S,SN` | 0.00 | both ends | `sleep` |
| 2982208 | `S,SN` | - | **transient** | `sleep` |
| 2982209 | `S,SN` | - | **transient** | `sleep` |
| 2982869 | `S,SN` | - | **transient** | `sleep` |
| 2982871 | `S,SN` | - | **transient** | `sleep` |
| 2982874 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.05 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `S1-screen-malloc.log` / `screen`  (2 snapshots)

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
| 50122 | `S,Sl` | 0.71 | both ends | `kwin_wayland` |
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
| 50658 | `S,Sl` | 0.01 | both ends | `mysqld` |
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
| 116643 | `S,Sl` | 0.03 | both ends | `kscreenlocker_g` |
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
| 1312476 | `S,Sl` | 0.02 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.26 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.01 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 3019769 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019770 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019771 | `S` | 0.00 | both ends | `systemd-userwor` |
| 3019856 | `S,SN` | - | **transient** | `sleep` |
| 3019882 | `S,SN` | - | **transient** | `sleep` |
| 3019914 | `S,SN` | - | **transient** | `sleep` |
| 3019919 | `S,SN` | - | **transient** | `sleep` |
| 3019923 | `S,SN` | - | **transient** | `sleep` |
| 3019926 | `S,SN` | - | **transient** | `sleep` |
| 3019928 | `S,SN` | - | **transient** | `sleep` |
| 3019930 | `S,SN` | - | **transient** | `sleep` |
| 3019944 | `S,SN` | - | **transient** | `sleep` |
| 3019960 | `S,SN` | - | **transient** | `sleep` |
| 3019961 | `S,SN` | - | **transient** | `sleep` |
| 3019963 | `S,SN` | - | **transient** | `sleep` |
| 3019965 | `S,SN` | - | **transient** | `sleep` |
| 3019968 | `S,SN` | - | **transient** | `sleep` |
| 3019976 | `Ss` | - | **transient** | `bwrap` |
| 3019989 | `Ss` | - | **transient** | `codex` |
| 3019997 | `S` | - | **transient** | `bash` |
| 3019998 | `R` | - | **transient** | `python3` |
| 3020664 | `S,SN` | - | **transient** | `sleep` |
| 3020668 | `S,SN` | - | **transient** | `sleep` |
| 3020670 | `S,SN` | - | **transient** | `sleep` |
| 3020672 | `S,SN` | - | **transient** | `sleep` |
| 3020687 | `S,SN` | - | **transient** | `sleep` |
| 3020737 | `S,SN` | - | **transient** | `sleep` |
| 3020740 | `S,SN` | - | **transient** | `sleep` |
| 3020742 | `S,SN` | - | **transient** | `sleep` |
| 3020778 | `S,SN` | - | **transient** | `sleep` |
| 3020794 | `S,SN` | - | **transient** | `sleep` |
| 3020796 | `S,SN` | - | **transient** | `sleep` |
| 3020797 | `S,SN` | - | **transient** | `sleep` |
| 3020799 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.06 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W1-wall-r1.log` / `wall-r1`  (8 snapshots)

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
| 1965 | `S,S<sl` | 0.00 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.00 | both ends | `wireplumber` |
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
| 50122 | `S,Sl` | 1.25 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.03 | both ends | `kded6` |
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
| 59463 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 116643 | `S,Sl` | 0.07 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.44 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.11 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.10 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.03 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.03 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2866677 | `SN` | - | **transient** | `sleep` |
| 2866679 | `S,SN` | - | **transient** | `sleep` |
| 2866690 | `S,SN` | - | **transient** | `sleep` |
| 2866694 | `S,SN` | - | **transient** | `sleep` |
| 2866716 | `S,SN` | - | **transient** | `sleep` |
| 2866719 | `S,SN` | - | **transient** | `sleep` |
| 2866722 | `S,SN` | - | **transient** | `sleep` |
| 2866811 | `S,SN` | - | **transient** | `sleep` |
| 2866815 | `S,SN` | - | **transient** | `sleep` |
| 2866816 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866818 | `S,SN` | - | **transient** | `sleep` |
| 2866825 | `S,SN` | 0.00 | both ends | `sleep` |
| 2866827 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866828 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866829 | `S,SN` | - | **transient** | `sleep` |
| 2866831 | `S,SN` | - | **transient** | `sleep` |
| 2867445 | `S,SN` | - | **transient** | `sleep` |
| 2867515 | `S,SN` | - | **transient** | `sleep` |
| 2867517 | `S,SN` | - | **transient** | `sleep` |
| 2867680 | `S,SN` | - | **transient** | `sleep` |
| 2868178 | `S,SN` | - | **transient** | `sleep` |
| 2868194 | `S,SN` | - | **transient** | `sleep` |
| 2868196 | `S,SN` | - | **transient** | `sleep` |
| 2868205 | `S,SN` | - | **transient** | `sleep` |
| 2868208 | `S,SN` | - | **transient** | `sleep` |
| 2868869 | `S,SN` | - | **transient** | `sleep` |
| 2868871 | `S,SN` | - | **transient** | `sleep` |
| 2868874 | `S,SN` | - | **transient** | `sleep` |
| 2869534 | `S,SN` | - | **transient** | `sleep` |
| 2869536 | `S,SN` | - | **transient** | `sleep` |
| 2870199 | `S,SN` | - | **transient** | `sleep` |
| 2870200 | `S,SN` | - | **transient** | `sleep` |
| 2870202 | `S,SN` | - | **transient** | `sleep` |
| 2870467 | `S,SN` | - | **transient** | `sleep` |
| 2870864 | `S,SN` | - | **transient** | `sleep` |
| 2870866 | `S,SN` | - | **transient** | `sleep` |
| 2870882 | `S,SN` | - | **transient** | `sleep` |
| 2870883 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.11 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W2-wall-r2.log` / `wall-r2`  (8 snapshots)

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
| 980 | `S,Ss` | 0.01 | both ends | `chronyd` |
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
| 50122 | `S,Sl` | 1.15 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.06 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.41 | both ends | `claude` |
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
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `Rl,S,Sl` | 0.19 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2866816 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866825 | `S,SN` | - | **transient** | `sleep` |
| 2866827 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866828 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2868869 | `S,SN` | - | **transient** | `sleep` |
| 2868874 | `S,SN` | - | **transient** | `sleep` |
| 2869536 | `S,SN` | - | **transient** | `sleep` |
| 2870199 | `S,SN` | - | **transient** | `sleep` |
| 2870200 | `S,SN` | - | **transient** | `sleep` |
| 2870202 | `S,SN` | - | **transient** | `sleep` |
| 2870467 | `S,SN` | - | **transient** | `sleep` |
| 2870864 | `S,SN` | - | **transient** | `sleep` |
| 2870866 | `S,SN` | - | **transient** | `sleep` |
| 2870882 | `S,SN` | - | **transient** | `sleep` |
| 2870883 | `S,SN` | - | **transient** | `sleep` |
| 2872187 | `S,SN` | - | **transient** | `sleep` |
| 2872866 | `S,SN` | - | **transient** | `sleep` |
| 2872868 | `S,SN` | - | **transient** | `sleep` |
| 2872885 | `S,SN` | - | **transient** | `sleep` |
| 2873530 | `S,SN` | - | **transient** | `sleep` |
| 2873533 | `S,SN` | - | **transient** | `sleep` |
| 2873534 | `S,SN` | - | **transient** | `sleep` |
| 2874196 | `S,SN` | - | **transient** | `sleep` |
| 2874199 | `S,SN` | - | **transient** | `sleep` |
| 2874201 | `S,SN` | - | **transient** | `sleep` |
| 2874862 | `S,SN` | - | **transient** | `sleep` |
| 2874872 | `S,SN` | - | **transient** | `sleep` |
| 2874874 | `S,SN` | - | **transient** | `sleep` |
| 2874883 | `S,SN` | - | **transient** | `sleep` |
| 2874885 | `S,SN` | - | **transient** | `sleep` |
| 2875124 | `S,SN` | - | **transient** | `sleep` |
| 2875544 | `S,SN` | - | **transient** | `sleep` |
| 2876160 | `S,SN` | - | **transient** | `sleep` |
| 2876205 | `S,SN` | - | **transient** | `sleep` |
| 2876207 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.10 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W3-wall-r3.log` / `wall-r3`  (8 snapshots)

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
| 999 | `S,Ssl` | 0.05 | both ends | `udisksd` |
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
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 1.13 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
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
| 116643 | `S,Sl` | 0.07 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.44 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.09 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.11 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.36 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.15 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2866816 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866827 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866828 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2873533 | `S,SN` | - | **transient** | `sleep` |
| 2874199 | `S,SN` | - | **transient** | `sleep` |
| 2874201 | `S,SN` | - | **transient** | `sleep` |
| 2874862 | `S,SN` | - | **transient** | `sleep` |
| 2874872 | `S,SN` | - | **transient** | `sleep` |
| 2874874 | `S,SN` | - | **transient** | `sleep` |
| 2874883 | `S,SN` | - | **transient** | `sleep` |
| 2874885 | `S,SN` | - | **transient** | `sleep` |
| 2875124 | `SN` | - | **transient** | `sleep` |
| 2875544 | `S,SN` | - | **transient** | `sleep` |
| 2876160 | `S,SN` | - | **transient** | `sleep` |
| 2876205 | `S,SN` | - | **transient** | `sleep` |
| 2876207 | `S,SN` | - | **transient** | `sleep` |
| 2877797 | `S,SN` | - | **transient** | `sleep` |
| 2878185 | `S,SN` | - | **transient** | `sleep` |
| 2878188 | `S,SN` | - | **transient** | `sleep` |
| 2878190 | `S,SN` | - | **transient** | `sleep` |
| 2878192 | `S,SN` | - | **transient** | `sleep` |
| 2878854 | `S,SN` | - | **transient** | `sleep` |
| 2878864 | `S,SN` | - | **transient** | `sleep` |
| 2878866 | `S,SN` | - | **transient** | `sleep` |
| 2878883 | `S,SN` | - | **transient** | `sleep` |
| 2879543 | `S,SN` | - | **transient** | `sleep` |
| 2879545 | `S,SN` | - | **transient** | `sleep` |
| 2879726 | `S,SN` | - | **transient** | `sleep` |
| 2880206 | `S,SN` | - | **transient** | `sleep` |
| 2880210 | `S,SN` | - | **transient** | `sleep` |
| 2880212 | `S,SN` | - | **transient** | `sleep` |
| 2880215 | `S,SN` | - | **transient** | `sleep` |
| 2880883 | `S,SN` | - | **transient** | `sleep` |
| 2880885 | `S,SN` | - | **transient** | `sleep` |
| 2880887 | `S,SN` | - | **transient** | `sleep` |
| 2881548 | `S,SN` | - | **transient** | `sleep` |
| 2881551 | `S,SN` | - | **transient** | `sleep` |
| 2881554 | `S,SN` | - | **transient** | `sleep` |
| 2882109 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.12 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W4-wall-r4.log` / `wall-r4`  (8 snapshots)

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
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
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
| 50122 | `R,S,Sl` | 1.41 | both ends | `kwin_wayland` |
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
| 50712 | `S,Ssl` | 0.01 | both ends | `kaccess` |
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
| 116643 | `S,Sl` | 0.07 | both ends | `kscreenlocker_g` |
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
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2866816 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866827 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866828 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2878883 | `S,SN` | - | **transient** | `sleep` |
| 2879545 | `S,SN` | - | **transient** | `sleep` |
| 2880206 | `S,SN` | - | **transient** | `sleep` |
| 2880210 | `S,SN` | - | **transient** | `sleep` |
| 2880212 | `S,SN` | - | **transient** | `sleep` |
| 2880215 | `S,SN` | - | **transient** | `sleep` |
| 2880883 | `S,SN` | - | **transient** | `sleep` |
| 2880885 | `S,SN` | - | **transient** | `sleep` |
| 2880887 | `S,SN` | - | **transient** | `sleep` |
| 2881548 | `S,SN` | - | **transient** | `sleep` |
| 2881551 | `S,SN` | - | **transient** | `sleep` |
| 2881554 | `S,SN` | - | **transient** | `sleep` |
| 2882109 | `S,SN` | - | **transient** | `sleep` |
| 2883547 | `S,SN` | - | **transient** | `sleep` |
| 2883549 | `S,SN` | - | **transient** | `sleep` |
| 2883550 | `S,SN` | - | **transient** | `sleep` |
| 2883552 | `S,SN` | - | **transient** | `sleep` |
| 2884207 | `S,SN` | - | **transient** | `sleep` |
| 2884209 | `S,SN` | - | **transient** | `sleep` |
| 2884211 | `S,SN` | - | **transient** | `sleep` |
| 2884222 | `S,SN` | - | **transient** | `sleep` |
| 2884224 | `S,SN` | - | **transient** | `sleep` |
| 2884886 | `S,SN` | - | **transient** | `sleep` |
| 2884888 | `S,SN` | - | **transient** | `sleep` |
| 2884891 | `S,SN` | - | **transient** | `sleep` |
| 2885251 | `S,SN` | - | **transient** | `sleep` |
| 2885554 | `S,SN` | - | **transient** | `sleep` |
| 2885556 | `S,SN` | - | **transient** | `sleep` |
| 2885572 | `S,SN` | - | **transient** | `sleep` |
| 2885573 | `S,SN` | - | **transient** | `sleep` |
| 2886234 | `S,SN` | - | **transient** | `sleep` |
| 2886237 | `S,SN` | - | **transient** | `sleep` |
| 2886239 | `S,SN` | - | **transient** | `sleep` |
| 2886903 | `S,SN` | - | **transient** | `sleep` |
| 2886905 | `S,SN` | - | **transient** | `sleep` |
| 2886908 | `S,SN` | - | **transient** | `sleep` |
| 2887159 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.15 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `W5-wall-r5.log` / `wall-r5`  (8 snapshots)

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
| 50122 | `S,Sl` | 1.20 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.06 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.45 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.11 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.08 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
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
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2866816 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866827 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2866828 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2884886 | `S,SN` | - | **transient** | `sleep` |
| 2884891 | `S,SN` | - | **transient** | `sleep` |
| 2885251 | `S,SN` | - | **transient** | `sleep` |
| 2885554 | `S,SN` | - | **transient** | `sleep` |
| 2885556 | `S,SN` | - | **transient** | `sleep` |
| 2885572 | `S,SN` | - | **transient** | `sleep` |
| 2886234 | `S,SN` | - | **transient** | `sleep` |
| 2886237 | `S,SN` | - | **transient** | `sleep` |
| 2886239 | `S,SN` | - | **transient** | `sleep` |
| 2886903 | `S,SN` | - | **transient** | `sleep` |
| 2886905 | `S,SN` | - | **transient** | `sleep` |
| 2886908 | `S,SN` | 0.00 | both ends | `sleep` |
| 2887159 | `S,SN` | - | **transient** | `sleep` |
| 2888886 | `S,SN` | - | **transient** | `sleep` |
| 2888890 | `S,SN` | - | **transient** | `sleep` |
| 2888892 | `S,SN` | - | **transient** | `sleep` |
| 2889330 | `S,SN` | - | **transient** | `sleep` |
| 2889553 | `S,SN` | - | **transient** | `sleep` |
| 2889555 | `S,SN` | - | **transient** | `sleep` |
| 2889571 | `S,SN` | - | **transient** | `sleep` |
| 2889573 | `S,SN` | - | **transient** | `sleep` |
| 2889583 | `S,SN` | - | **transient** | `sleep` |
| 2890244 | `S,SN` | - | **transient** | `sleep` |
| 2890247 | `S,SN` | - | **transient** | `sleep` |
| 2890249 | `S,SN` | - | **transient** | `sleep` |
| 2890910 | `S,SN` | - | **transient** | `sleep` |
| 2891359 | `S,SN` | - | **transient** | `sleep` |
| 2891569 | `S,SN` | - | **transient** | `sleep` |
| 2891572 | `S,SN` | - | **transient** | `sleep` |
| 2891574 | `S,SN` | - | **transient** | `sleep` |
| 2891576 | `S,SN` | - | **transient** | `sleep` |
| 2892237 | `S,SN` | - | **transient** | `sleep` |
| 2892242 | `S,SN` | - | **transient** | `sleep` |
| 2892251 | `S,SN` | - | **transient** | `sleep` |
| 2892260 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.15 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `X1-xwall-r1.log` / `xwall-r1`  (6 snapshots)

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
| 1238 | `S,Ss` | 0.01 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
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
| 116643 | `S,Sl` | 0.06 | both ends | `kscreenlocker_g` |
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
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.35 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.12 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2957339 | `S,SN` | - | **transient** | `sleep` |
| 2958666 | `S,SN` | - | **transient** | `sleep` |
| 2958669 | `S,SN` | - | **transient** | `sleep` |
| 2958672 | `S,SN` | - | **transient** | `sleep` |
| 2959979 | `SN` | - | **transient** | `sleep` |
| 2959981 | `S,SN` | - | **transient** | `sleep` |
| 2959997 | `S,SN` | - | **transient** | `sleep` |
| 2960000 | `S,SN` | - | **transient** | `sleep` |
| 2960002 | `S,SN` | - | **transient** | `sleep` |
| 2960009 | `S,SN` | - | **transient** | `sleep` |
| 2960011 | `S,SN` | - | **transient** | `sleep` |
| 2960013 | `S,SN` | - | **transient** | `sleep` |
| 2960014 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960015 | `S,SN` | - | **transient** | `sleep` |
| 2960016 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960017 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960482 | `S,SN` | - | **transient** | `sleep` |
| 2960699 | `S,SN` | - | **transient** | `sleep` |
| 2960702 | `S,SN` | - | **transient** | `sleep` |
| 2960704 | `S,SN` | - | **transient** | `sleep` |
| 2961366 | `S,SN` | - | **transient** | `sleep` |
| 2961382 | `S,SN` | - | **transient** | `sleep` |
| 2961391 | `S,SN` | - | **transient** | `sleep` |
| 2961393 | `S,SN` | - | **transient** | `sleep` |
| 2961394 | `S,SN` | - | **transient** | `sleep` |
| 2961725 | `S,SN` | - | **transient** | `sleep` |
| 2962056 | `S,SN` | - | **transient** | `sleep` |
| 2962339 | `S,SN` | - | **transient** | `sleep` |
| 2962716 | `S,SN` | - | **transient** | `sleep` |
| 2962718 | `S,SN` | - | **transient** | `sleep` |
| 2962721 | `S,SN` | - | **transient** | `sleep` |
| 2962723 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.08 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `X2-xwall-r2.log` / `xwall-r2`  (6 snapshots)

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
| 50122 | `S,Sl` | 0.94 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.01 | both ends | `akonadiserver` |
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
| 59463 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 1097257 | `S,Sl+` | 0.33 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.07 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
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
| 2722044 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2960014 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960016 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960017 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960699 | `S,SN` | - | **transient** | `sleep` |
| 2960704 | `S,SN` | - | **transient** | `sleep` |
| 2961366 | `S,SN` | - | **transient** | `sleep` |
| 2961382 | `S,SN` | - | **transient** | `sleep` |
| 2961391 | `S,SN` | - | **transient** | `sleep` |
| 2961393 | `S,SN` | - | **transient** | `sleep` |
| 2961725 | `S,SN` | - | **transient** | `sleep` |
| 2962056 | `S,SN` | - | **transient** | `sleep` |
| 2962339 | `S,SN` | - | **transient** | `sleep` |
| 2962716 | `S,SN` | - | **transient** | `sleep` |
| 2962718 | `S,SN` | 0.00 | both ends | `sleep` |
| 2962721 | `S,SN` | - | **transient** | `sleep` |
| 2962723 | `S,SN` | - | **transient** | `sleep` |
| 2964697 | `S,SN` | - | **transient** | `sleep` |
| 2964699 | `S,SN` | - | **transient** | `sleep` |
| 2964702 | `S,SN` | - | **transient** | `sleep` |
| 2964716 | `S,SN` | - | **transient** | `sleep` |
| 2964718 | `S,SN` | - | **transient** | `sleep` |
| 2964727 | `S,SN` | - | **transient** | `sleep` |
| 2965395 | `S,SN` | - | **transient** | `sleep` |
| 2965396 | `S,SN` | - | **transient** | `sleep` |
| 2965398 | `S,SN` | - | **transient** | `sleep` |
| 2965400 | `S,SN` | - | **transient** | `sleep` |
| 2966061 | `S,SN` | - | **transient** | `sleep` |
| 2966063 | `S,SN` | - | **transient** | `sleep` |
| 2966065 | `S,SN` | - | **transient** | `sleep` |
| 2966725 | `S,SN` | - | **transient** | `sleep` |
| 2966728 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `R,S,Ssl` | 0.08 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `X3-xwall-r3.log` / `xwall-r3`  (6 snapshots)

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
| 1097257 | `S,Sl+` | 0.30 | both ends | `claude` |
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
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2960014 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960016 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960017 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2962718 | `S,SN` | - | **transient** | `sleep` |
| 2964699 | `SN` | - | **transient** | `sleep` |
| 2964702 | `S,SN` | - | **transient** | `sleep` |
| 2964716 | `S,SN` | - | **transient** | `sleep` |
| 2964727 | `S,SN` | - | **transient** | `sleep` |
| 2965395 | `S,SN` | - | **transient** | `sleep` |
| 2965398 | `S,SN` | - | **transient** | `sleep` |
| 2965400 | `S,SN` | - | **transient** | `sleep` |
| 2966061 | `S,SN` | - | **transient** | `sleep` |
| 2966063 | `S,SN` | - | **transient** | `sleep` |
| 2966065 | `S,SN` | - | **transient** | `sleep` |
| 2966725 | `S,SN` | - | **transient** | `sleep` |
| 2966728 | `S,SN` | - | **transient** | `sleep` |
| 2968257 | `S,SN` | - | **transient** | `sleep` |
| 2968706 | `S,SN` | - | **transient** | `sleep` |
| 2968709 | `S,SN` | - | **transient** | `sleep` |
| 2968712 | `S,SN` | - | **transient** | `sleep` |
| 2969381 | `S,SN` | - | **transient** | `sleep` |
| 2969390 | `S,SN` | - | **transient** | `sleep` |
| 2969392 | `S,SN` | - | **transient** | `sleep` |
| 2969393 | `S,SN` | - | **transient** | `sleep` |
| 2969395 | `S,SN` | - | **transient** | `sleep` |
| 2970059 | `S,SN` | - | **transient** | `sleep` |
| 2970061 | `S,SN` | - | **transient** | `sleep` |
| 2970063 | `S,SN` | - | **transient** | `sleep` |
| 2970065 | `S,SN` | - | **transient** | `sleep` |
| 2970282 | `S,SN` | - | **transient** | `sleep` |
| 2970725 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.08 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `X4-xwall-r4.log` / `xwall-r4`  (6 snapshots)

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
| 50122 | `S,Sl` | 0.82 | both ends | `kwin_wayland` |
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
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.31 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.09 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.02 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2960014 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960016 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2960017 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2968257 | `S,SN` | - | **transient** | `sleep` |
| 2968709 | `S,SN` | - | **transient** | `sleep` |
| 2968712 | `S,SN` | - | **transient** | `sleep` |
| 2969381 | `S,SN` | - | **transient** | `sleep` |
| 2969392 | `S,SN` | - | **transient** | `sleep` |
| 2969395 | `S,SN` | - | **transient** | `sleep` |
| 2970059 | `S,SN` | - | **transient** | `sleep` |
| 2970061 | `S,SN` | - | **transient** | `sleep` |
| 2970063 | `S,SN` | - | **transient** | `sleep` |
| 2970065 | `S,SN` | 0.00 | both ends | `sleep` |
| 2970282 | `S,SN` | - | **transient** | `sleep` |
| 2970725 | `S,SN` | - | **transient** | `sleep` |
| 2972046 | `S,SN` | - | **transient** | `sleep` |
| 2972710 | `S,SN` | - | **transient** | `sleep` |
| 2972713 | `S,SN` | - | **transient** | `sleep` |
| 2972716 | `S,SN` | - | **transient** | `sleep` |
| 2973377 | `S,SN` | - | **transient** | `sleep` |
| 2973386 | `S,SN` | - | **transient** | `sleep` |
| 2973395 | `S,SN` | - | **transient** | `sleep` |
| 2973396 | `S,SN` | - | **transient** | `sleep` |
| 2973398 | `S,SN` | - | **transient** | `sleep` |
| 2973400 | `S,SN` | - | **transient** | `sleep` |
| 2974069 | `S,SN` | - | **transient** | `sleep` |
| 2974071 | `S,SN` | - | **transient** | `sleep` |
| 2974073 | `S,SN` | - | **transient** | `sleep` |
| 2974733 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.07 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `X5-xwall-r5.log` / `xwall-r5`  (6 snapshots)

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
| 50122 | `S,Sl` | 0.80 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.30 | both ends | `claude` |
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
| 2721602 | `S,Ssl` | 0.27 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2925849 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926509 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926510 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2949970 | `S,SN` | - | **transient** | `sleep` |
| 2952632 | `S,SN` | - | **transient** | `sleep` |
| 2952648 | `S,SN` | - | **transient** | `sleep` |
| 2953313 | `S,SN` | - | **transient** | `sleep` |
| 2953982 | `S,SN` | - | **transient** | `sleep` |
| 2953984 | `S,SN` | - | **transient** | `sleep` |
| 2953986 | `S,SN` | - | **transient** | `sleep` |
| 2954278 | `S,SN` | - | **transient** | `sleep` |
| 2954653 | `S,SN` | - | **transient** | `sleep` |
| 2954656 | `S,SN` | - | **transient** | `sleep` |
| 2954658 | `S,SN` | - | **transient** | `sleep` |
| 2954741 | `S,SN` | - | **transient** | `sleep` |
| 2955955 | `S,SN` | - | **transient** | `sleep` |
| 2956650 | `S,SN` | - | **transient** | `sleep` |
| 2956666 | `S,SN` | - | **transient** | `sleep` |
| 2956668 | `S,SN` | - | **transient** | `sleep` |
| 2956669 | `S,SN` | - | **transient** | `sleep` |
| 2956671 | `S,SN` | - | **transient** | `sleep` |
| 2957333 | `S,SN` | - | **transient** | `sleep` |
| 2957335 | `S,SN` | - | **transient** | `sleep` |
| 2957337 | `S,SN` | - | **transient** | `sleep` |
| 2957339 | `S,SN` | - | **transient** | `sleep` |
| 2957341 | `S,SN` | - | **transient** | `sleep` |
| 2958003 | `S,SN` | - | **transient** | `sleep` |
| 2958021 | `S,SN` | - | **transient** | `sleep` |
| 2958666 | `S,SN` | - | **transient** | `sleep` |
| 2958669 | `S,SN` | - | **transient** | `sleep` |
| 2958672 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.07 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `Y1-ywall-r1.log` / `ywall-r1`  (7 snapshots)

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
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.01 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.00 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.00 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.01 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.01 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.02 | both ends | `wireplumber` |
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
| 50122 | `S,Sl` | 0.96 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
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
| 1097257 | `S,Sl+` | 0.42 | both ends | `claude` |
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
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.03 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.03 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `Rl,S,Sl` | 0.18 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2984263 | `S` | - | **transient** | `systemd-userwor` |
| 2984265 | `S` | - | **transient** | `systemd-userwor` |
| 2984266 | `S` | - | **transient** | `systemd-userwor` |
| 2985166 | `SN` | - | **transient** | `sleep` |
| 2985169 | `S,SN` | - | **transient** | `sleep` |
| 2985174 | `S,SN` | - | **transient** | `sleep` |
| 2985209 | `S,SN` | - | **transient** | `sleep` |
| 2985212 | `S,SN` | - | **transient** | `sleep` |
| 2985216 | `S,SN` | - | **transient** | `sleep` |
| 2985218 | `S,SN` | - | **transient** | `sleep` |
| 2985220 | `S,SN` | - | **transient** | `sleep` |
| 2985223 | `S,SN` | 0.00 | both ends | `sleep` |
| 2985225 | `S,SN` | - | **transient** | `sleep` |
| 2985228 | `S,SN` | - | **transient** | `sleep` |
| 2985242 | `S,SN` | - | **transient** | `sleep` |
| 2985248 | `S,SN` | - | **transient** | `sleep` |
| 2985762 | `S,SN` | - | **transient** | `sleep` |
| 2985932 | `S,SN` | - | **transient** | `sleep` |
| 2985935 | `S,SN` | - | **transient** | `sleep` |
| 2986610 | `S,SN` | - | **transient** | `sleep` |
| 2986612 | `S,SN` | - | **transient** | `sleep` |
| 2986613 | `S,SN` | - | **transient** | `sleep` |
| 2986615 | `S,SN` | - | **transient** | `sleep` |
| 2986617 | `S,SN` | - | **transient** | `sleep` |
| 2986619 | `S,SN` | - | **transient** | `sleep` |
| 2987283 | `S,SN` | - | **transient** | `sleep` |
| 2987285 | `S,SN` | - | **transient** | `sleep` |
| 2987287 | `S,SN` | - | **transient** | `sleep` |
| 2987947 | `S,SN` | - | **transient** | `sleep` |
| 2987948 | `S` | - | **transient** | `systemd-userwor` |
| 2987949 | `S` | - | **transient** | `systemd-userwor` |
| 2987950 | `S` | - | **transient** | `systemd-userwor` |
| 2987952 | `S,SN` | - | **transient** | `sleep` |
| 2988613 | `S,SN` | - | **transient** | `sleep` |
| 2988615 | `S,SN` | - | **transient** | `sleep` |
| 2988617 | `S,SN` | - | **transient** | `sleep` |
| 2988621 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.13 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `Y2-ywall-r2.log` / `ywall-r2`  (7 snapshots)

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
| 50122 | `S,Sl` | 0.97 | both ends | `kwin_wayland` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.42 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.10 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.02 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.75 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.02 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
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
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.17 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | - | **transient** | `sleep` |
| 2985223 | `S,SN` | - | **transient** | `sleep` |
| 2986612 | `S,SN` | - | **transient** | `sleep` |
| 2986615 | `S,SN` | - | **transient** | `sleep` |
| 2986617 | `S,SN` | - | **transient** | `sleep` |
| 2987283 | `S,SN` | - | **transient** | `sleep` |
| 2987285 | `S,SN` | - | **transient** | `sleep` |
| 2987287 | `S,SN` | - | **transient** | `sleep` |
| 2987947 | `S,SN` | - | **transient** | `sleep` |
| 2987948 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987950 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987952 | `S,SN` | - | **transient** | `sleep` |
| 2988613 | `S,SN` | - | **transient** | `sleep` |
| 2988615 | `S,SN` | - | **transient** | `sleep` |
| 2988617 | `S,SN` | - | **transient** | `sleep` |
| 2988621 | `S,SN` | - | **transient** | `sleep` |
| 2990620 | `S,SN` | - | **transient** | `sleep` |
| 2990621 | `S,SN` | - | **transient** | `sleep` |
| 2990623 | `S,SN` | - | **transient** | `sleep` |
| 2990625 | `S,SN` | - | **transient** | `sleep` |
| 2990627 | `S,SN` | - | **transient** | `sleep` |
| 2991336 | `S,SNsl` | - | **transient** | `node-22` |
| 2991359 | `S,SN` | - | **transient** | `sleep` |
| 2991474 | `S,Ss` | - | **transient** | `systemd-hostnam` |
| 2991509 | `S,SN` | - | **transient** | `sleep` |
| 2991511 | `S,SN` | - | **transient** | `sleep` |
| 2991571 | `S,SN` | - | **transient** | `sleep` |
| 2991573 | `S,SN` | - | **transient** | `sleep` |
| 2992262 | `S,SN` | - | **transient** | `sleep` |
| 2992263 | `S,SN` | - | **transient** | `sleep` |
| 2992983 | `S,SN` | - | **transient** | `sleep` |
| 2992987 | `S,SN` | - | **transient** | `sleep` |
| 2992989 | `S,SN` | - | **transient** | `sleep` |
| 2993736 | `S,SN` | - | **transient** | `sleep` |
| 2993753 | `S,SN` | - | **transient** | `sleep` |
| 2993754 | `S,SN` | - | **transient** | `sleep` |
| 2993755 | `S,SN` | - | **transient** | `sleep` |
| 2993757 | `S,SN` | - | **transient** | `sleep` |
| 2993759 | `S,SN` | - | **transient** | `sleep` |
| 2993888 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.12 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `Y3-ywall-r3.log` / `ywall-r3`  (7 snapshots)

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
| 50122 | `S,Sl` | 0.99 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.43 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.09 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.59 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.04 | both ends | `codex-code-mode` |
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
| 2721602 | `S,Ssl` | 0.03 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2987948 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987950 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2991336 | `S,SNsl` | 0.00 | both ends | `node-22` |
| 2991359 | `S,SN` | - | **transient** | `sleep` |
| 2991474 | `S,Ss` | - | **transient** | `systemd-hostnam` |
| 2991511 | `S,SN` | - | **transient** | `sleep` |
| 2991571 | `S,SN` | 0.00 | both ends | `sleep` |
| 2991573 | `S,SN` | - | **transient** | `sleep` |
| 2992983 | `S,SN` | - | **transient** | `sleep` |
| 2992987 | `S,SN` | - | **transient** | `sleep` |
| 2992989 | `S,SN` | - | **transient** | `sleep` |
| 2993736 | `S,SN` | - | **transient** | `sleep` |
| 2993753 | `S,SN` | - | **transient** | `sleep` |
| 2993754 | `S,SN` | - | **transient** | `sleep` |
| 2993755 | `S,SN` | - | **transient** | `sleep` |
| 2993757 | `S,SN` | - | **transient** | `sleep` |
| 2993759 | `S,SN` | - | **transient** | `sleep` |
| 2993888 | `S,SN` | - | **transient** | `sleep` |
| 2995809 | `S,SN` | - | **transient** | `sleep` |
| 2996013 | `S,SN` | - | **transient** | `sleep` |
| 2996297 | `S,SN` | - | **transient** | `sleep` |
| 2996496 | `S,SN` | - | **transient** | `sleep` |
| 2997167 | `S,SN` | - | **transient** | `sleep` |
| 2997186 | `S,SN` | - | **transient** | `sleep` |
| 2997188 | `S,SN` | - | **transient** | `sleep` |
| 2997190 | `S,SN` | - | **transient** | `sleep` |
| 2997193 | `S,SN` | - | **transient** | `sleep` |
| 2997876 | `S,SN` | - | **transient** | `sleep` |
| 2997893 | `S,SN` | - | **transient** | `sleep` |
| 2997894 | `S,SN` | - | **transient** | `sleep` |
| 2997896 | `S,SN` | - | **transient** | `sleep` |
| 2997898 | `S,SN` | - | **transient** | `sleep` |
| 2998504 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.10 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `Y4-ywall-r4.log` / `ywall-r4`  (7 snapshots)

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
| 50122 | `S,Sl` | 0.97 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.01 | both ends | `akonadi_control` |
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
| 1097257 | `S,Sl+` | 0.43 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.09 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.35 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.02 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.27 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.17 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2987948 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987950 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2991336 | `S,SNsl` | 0.01 | both ends | `node-22` |
| 2991571 | `S,SN` | - | **transient** | `sleep` |
| 2996297 | `S,SN` | - | **transient** | `sleep` |
| 2997186 | `S,SN` | - | **transient** | `sleep` |
| 2997188 | `S,SN` | - | **transient** | `sleep` |
| 2997190 | `S,SN` | - | **transient** | `sleep` |
| 2997193 | `S,SN` | - | **transient** | `sleep` |
| 2997876 | `S,SN` | - | **transient** | `sleep` |
| 2997893 | `S,SN` | - | **transient** | `sleep` |
| 2997894 | `S,SN` | - | **transient** | `sleep` |
| 2997896 | `S,SN` | - | **transient** | `sleep` |
| 2997898 | `S,SN` | - | **transient** | `sleep` |
| 2998504 | `S,SN` | - | **transient** | `sleep` |
| 2999407 | `S,SN` | - | **transient** | `sleep` |
| 2999655 | `S,SN` | - | **transient** | `sleep` |
| 3000590 | `S,SN` | - | **transient** | `sleep` |
| 3000592 | `S,SN` | - | **transient** | `sleep` |
| 3000596 | `S,SN` | - | **transient** | `sleep` |
| 3000597 | `S,SN` | - | **transient** | `sleep` |
| 3001299 | `S,SN` | - | **transient** | `sleep` |
| 3001303 | `S,SN` | - | **transient** | `sleep` |
| 3001305 | `S,SN` | - | **transient** | `sleep` |
| 3001970 | `S,SN` | - | **transient** | `sleep` |
| 3001987 | `S,SN` | - | **transient** | `sleep` |
| 3001988 | `S,SN` | - | **transient** | `sleep` |
| 3002004 | `S,SN` | - | **transient** | `sleep` |
| 3002073 | `S,SN` | - | **transient** | `sleep` |
| 3002667 | `S,SN` | - | **transient** | `sleep` |
| 3002669 | `S,SN` | - | **transient** | `sleep` |
| 3003335 | `S,SN` | - | **transient** | `sleep` |
| 3003337 | `S,SN` | - | **transient** | `sleep` |
| 3003339 | `S,SN` | - | **transient** | `sleep` |
| 3003341 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.10 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `Y5-ywall-r5.log` / `ywall-r5`  (7 snapshots)

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
| 50122 | `Rl,S,Sl` | 1.18 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.07 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 0.47 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.09 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.01 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.42 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.02 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.34 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.14 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `R,S,Sl` | 0.19 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2987948 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987949 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2987950 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2991336 | `S,SNsl` | 0.03 | both ends | `node-22` |
| 3000590 | `S,SN` | - | **transient** | `sleep` |
| 3001303 | `S,SN` | - | **transient** | `sleep` |
| 3001305 | `S,SN` | - | **transient** | `sleep` |
| 3001970 | `S,SN` | - | **transient** | `sleep` |
| 3001987 | `S,SN` | - | **transient** | `sleep` |
| 3001988 | `S,SN` | - | **transient** | `sleep` |
| 3002073 | `S,SN` | - | **transient** | `sleep` |
| 3002667 | `S,SN` | - | **transient** | `sleep` |
| 3002669 | `S,SN` | - | **transient** | `sleep` |
| 3003335 | `S,SN` | - | **transient** | `sleep` |
| 3003337 | `S,SN` | - | **transient** | `sleep` |
| 3003339 | `S,SN` | - | **transient** | `sleep` |
| 3003341 | `S,SN` | - | **transient** | `sleep` |
| 3004046 | `S,SN` | - | **transient** | `sleep` |
| 3005361 | `S,SN` | - | **transient** | `sleep` |
| 3005363 | `S,SN` | - | **transient** | `sleep` |
| 3005483 | `S,SN` | - | **transient** | `sleep` |
| 3005630 | `S,SN` | - | **transient** | `sleep` |
| 3006032 | `S,SN` | - | **transient** | `sleep` |
| 3006048 | `S,SN` | - | **transient** | `sleep` |
| 3006705 | `S,SN` | - | **transient** | `sleep` |
| 3006707 | `S,SN` | - | **transient** | `sleep` |
| 3006709 | `S,SN` | - | **transient** | `sleep` |
| 3006711 | `S,SN` | - | **transient** | `sleep` |
| 3006723 | `S,SN` | - | **transient** | `sleep` |
| 3006725 | `S,SN` | - | **transient** | `sleep` |
| 3006728 | `S,SN` | - | **transient** | `sleep` |
| 3006730 | `S,SN` | - | **transient** | `sleep` |
| 3007396 | `S,SN` | - | **transient** | `sleep` |
| 3007405 | `S,SN` | - | **transient** | `sleep` |
| 3008109 | `S,SN` | - | **transient** | `sleep` |
| 3008112 | `S,SN` | - | **transient** | `sleep` |
| 3008115 | `S,SN` | - | **transient** | `sleep` |
| 3008118 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.13 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |
