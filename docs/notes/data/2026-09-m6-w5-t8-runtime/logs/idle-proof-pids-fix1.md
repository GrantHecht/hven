# Every foreign pid of every batch, with every state observed for it

Settler ruling R13 (W5 T8.9r fix round 3): R2' asks for every foreign pid and state seen in any snapshot, listed -- not a count and a busiest-five. This is that list, written by `scripts/idle_proof.py` from the same snapshots the fractions are computed from.

States are the UNION of `/proc/<pid>/stat`'s single character (`FOREIGN_TICK`) and `ps`'s full string (`FOREIGN_PS`). A pid marked **transient** was present in some snapshot of the batch but not in both ends, so it has no CPU delta. `delta` is ticks/clk across the batch window for pids present at both ends.

## `L4-leg1-ipm-r1.log` / `leg1-ipm-r1`  (4 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.02 | both ends | `systemd-journal` |
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
| 1040 | `S,Ss` | 0.01 | both ends | `systemd-logind` |
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
| 1244 | `S,Ssl` | 0.05 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.00 | both ends | `tuned` |
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
| 2236 | `S,S<Lsl` | 0.01 | both ends | `pipewire-pulse` |
| 2293 | `S,Ss` | 0.01 | both ends | `obexd` |
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
| 50122 | `S,Sl` | 1.42 | both ends | `kwin_wayland` |
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
| 50718 | `S,Ssl` | 0.01 | both ends | `polkit-kde-auth` |
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
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.54 | both ends | `claude` |
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
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2491874 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491876 | `S,SN` | - | **transient** | `sleep` |
| 2491877 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491878 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491915 | `S,SN` | - | **transient** | `sleep` |
| 2491917 | `S,SN` | - | **transient** | `sleep` |
| 2491929 | `S,SN` | - | **transient** | `sleep` |
| 2491941 | `S,SN` | - | **transient** | `sleep` |
| 2491943 | `S,SN` | - | **transient** | `sleep` |
| 2491952 | `S,SN` | 0.00 | both ends | `sleep` |
| 2491953 | `S,SN` | - | **transient** | `sleep` |
| 2491955 | `S,SN` | - | **transient** | `sleep` |
| 2491958 | `S,SN` | - | **transient** | `sleep` |
| 2491970 | `S,SN` | - | **transient** | `sleep` |
| 2492703 | `S,SN` | - | **transient** | `sleep` |
| 2492708 | `S,SN` | - | **transient** | `sleep` |
| 2492713 | `S,SN` | - | **transient** | `sleep` |
| 2492715 | `S,SN` | - | **transient** | `sleep` |
| 2492718 | `S,SN` | - | **transient** | `sleep` |
| 2492720 | `S,SN` | - | **transient** | `sleep` |
| 2492722 | `S,SN` | - | **transient** | `sleep` |
| 2493369 | `S,SN` | - | **transient** | `sleep` |
| 2493380 | `S,SN` | - | **transient** | `sleep` |
| 2493383 | `S,SN` | - | **transient** | `sleep` |
| 2493390 | `S,SN` | - | **transient** | `sleep` |
| 2493412 | `S,SN` | - | **transient** | `sleep` |
| 2493414 | `S,SN` | - | **transient** | `sleep` |
| 2493461 | `S,SN` | - | **transient** | `sleep` |
| 2493465 | `S,Ssl` | - | **transient** | `tailscaled` |
| 2493472 | `S` | - | **transient** | `fish` |
| 2493569 | `S,Ss` | - | **transient** | `systemd-hostnam` |
| 2493617 | `S,S+` | - | **transient** | `tmux:` |
| 2721602 | `S,Ssl` | 0.28 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.13 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-ipm-r2.log` / `leg1-ipm-r2`  (4 snapshots)

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
| 1244 | `S,Ssl` | 0.08 | both ends | `tailscaled` |
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
| 2082 | `S` | 0.01 | both ends | `dbus-broker` |
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
| 50122 | `S,Sl` | 1.40 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.53 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
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
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2491874 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491877 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491878 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491952 | `S,SN` | - | **transient** | `sleep` |
| 2492718 | `S,SN` | - | **transient** | `sleep` |
| 2492720 | `S,SN` | - | **transient** | `sleep` |
| 2492722 | `S,SN` | - | **transient** | `sleep` |
| 2493369 | `S,SN` | - | **transient** | `sleep` |
| 2493380 | `S,SN` | - | **transient** | `sleep` |
| 2493383 | `S,SN` | - | **transient** | `sleep` |
| 2493390 | `S,SN` | - | **transient** | `sleep` |
| 2493412 | `S,SN` | - | **transient** | `sleep` |
| 2493414 | `S,SN` | - | **transient** | `sleep` |
| 2493461 | `S,SN` | - | **transient** | `sleep` |
| 2493465 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
| 2493472 | `S` | 0.00 | both ends | `fish` |
| 2493569 | `S,Ss` | - | **transient** | `systemd-hostnam` |
| 2493617 | `S,S+` | 0.00 | both ends | `tmux:` |
| 2495570 | `S,SN` | - | **transient** | `sleep` |
| 2495577 | `S,SN` | - | **transient** | `sleep` |
| 2495584 | `S,SN` | - | **transient** | `sleep` |
| 2495587 | `S,SN` | - | **transient** | `sleep` |
| 2495590 | `S,SN` | - | **transient** | `sleep` |
| 2495596 | `S,SN` | - | **transient** | `sleep` |
| 2495598 | `S,SN` | - | **transient** | `sleep` |
| 2495621 | `S,SN` | - | **transient** | `sleep` |
| 2495625 | `S,SN` | - | **transient** | `sleep` |
| 2496287 | `S,SN` | - | **transient** | `sleep` |
| 2496303 | `S,SN` | - | **transient** | `sleep` |
| 2496306 | `S,SN` | - | **transient** | `sleep` |
| 2496310 | `S,SN` | - | **transient** | `sleep` |
| 2496315 | `S,SN` | - | **transient** | `sleep` |
| 2496318 | `S,SN` | - | **transient** | `sleep` |
| 2496320 | `S,SN` | - | **transient** | `sleep` |
| 2496366 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.02 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.17 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-ipm-r3.log` / `leg1-ipm-r3`  (4 snapshots)

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
| 1244 | `S,Ssl` | 0.04 | both ends | `tailscaled` |
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
| 50122 | `R,S,Sl` | 1.55 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.53 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.04 | both ends | `codex` |
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
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2491874 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491877 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491878 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2493465 | `S,Ssl` | - | **transient** | `tailscaled` |
| 2493472 | `S` | - | **transient** | `fish` |
| 2493617 | `S,S+` | - | **transient** | `tmux:` |
| 2495584 | `S,SN` | - | **transient** | `sleep` |
| 2495621 | `S,SN` | - | **transient** | `sleep` |
| 2495625 | `S,SN` | - | **transient** | `sleep` |
| 2496287 | `S,SN` | - | **transient** | `sleep` |
| 2496303 | `S,SN` | - | **transient** | `sleep` |
| 2496306 | `S,SN` | - | **transient** | `sleep` |
| 2496310 | `S,SN` | - | **transient** | `sleep` |
| 2496315 | `S,SN` | - | **transient** | `sleep` |
| 2496318 | `S,SN` | - | **transient** | `sleep` |
| 2496320 | `S,SN` | - | **transient** | `sleep` |
| 2496366 | `S,SN` | - | **transient** | `sleep` |
| 2498315 | `S,SN` | - | **transient** | `sleep` |
| 2498319 | `S,SN` | - | **transient** | `sleep` |
| 2498344 | `S,SN` | - | **transient** | `sleep` |
| 2498359 | `S,SN` | - | **transient** | `sleep` |
| 2498363 | `S,SN` | - | **transient** | `sleep` |
| 2498367 | `S,SN` | - | **transient** | `sleep` |
| 2498369 | `S,SN` | - | **transient** | `sleep` |
| 2498372 | `S,SN` | - | **transient** | `sleep` |
| 2498376 | `S,SN` | - | **transient** | `sleep` |
| 2498378 | `S,SN` | - | **transient** | `sleep` |
| 2499070 | `S,SN` | - | **transient** | `sleep` |
| 2499078 | `S,SN` | - | **transient** | `sleep` |
| 2499086 | `S,SN` | - | **transient** | `sleep` |
| 2499088 | `S,SN` | - | **transient** | `sleep` |
| 2499093 | `S,SN` | - | **transient** | `sleep` |
| 2499096 | `S,SN` | - | **transient** | `sleep` |
| 2499099 | `S,SN` | - | **transient** | `sleep` |
| 2499102 | `S,SN` | - | **transient** | `sleep` |
| 2499105 | `S,SN` | - | **transient** | `sleep` |
| 2499108 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.14 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.16 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-ssn-r1.log` / `leg1-ssn-r1`  (4 snapshots)

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
| 50122 | `S,Sl` | 1.06 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.42 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.06 | both ends | `codex` |
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
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2491874 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491877 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491878 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2498367 | `S,SN` | - | **transient** | `sleep` |
| 2499070 | `S,SN` | - | **transient** | `sleep` |
| 2499078 | `S,SN` | - | **transient** | `sleep` |
| 2499086 | `S,SN` | - | **transient** | `sleep` |
| 2499088 | `S,SN` | - | **transient** | `sleep` |
| 2499093 | `S,SN` | 0.00 | both ends | `sleep` |
| 2499096 | `S,SN` | - | **transient** | `sleep` |
| 2499099 | `S,SN` | - | **transient** | `sleep` |
| 2499102 | `S,SN` | - | **transient** | `sleep` |
| 2499105 | `S,SN` | - | **transient** | `sleep` |
| 2499108 | `S,SN` | - | **transient** | `sleep` |
| 2501030 | `S,SN` | - | **transient** | `sleep` |
| 2501056 | `S,SN` | - | **transient** | `sleep` |
| 2501060 | `S,SN` | - | **transient** | `sleep` |
| 2501065 | `S,SN` | - | **transient** | `sleep` |
| 2501437 | `S,SN` | - | **transient** | `sleep` |
| 2501725 | `S,SN` | - | **transient** | `sleep` |
| 2501734 | `S,SN` | - | **transient** | `sleep` |
| 2501738 | `S,SN` | - | **transient** | `sleep` |
| 2501741 | `S,SN` | - | **transient** | `sleep` |
| 2501747 | `S,SN` | - | **transient** | `sleep` |
| 2501750 | `S,SN` | - | **transient** | `sleep` |
| 2501775 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.27 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.06 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.04 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.18 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.11 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-ssn-r2.log` / `leg1-ssn-r2`  (4 snapshots)

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
| 50122 | `S,Sl` | 1.01 | both ends | `kwin_wayland` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.41 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.03 | both ends | `codex` |
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
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2491874 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491877 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491878 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2499093 | `S,SN` | - | **transient** | `sleep` |
| 2501060 | `SN` | - | **transient** | `sleep` |
| 2501065 | `S,SN` | - | **transient** | `sleep` |
| 2501437 | `S,SN` | - | **transient** | `sleep` |
| 2501725 | `S,SN` | - | **transient** | `sleep` |
| 2501734 | `S,SN` | 0.00 | both ends | `sleep` |
| 2501738 | `S,SN` | - | **transient** | `sleep` |
| 2501741 | `S,SN` | - | **transient** | `sleep` |
| 2501747 | `S,SN` | - | **transient** | `sleep` |
| 2501750 | `S,SN` | - | **transient** | `sleep` |
| 2501775 | `S,SN` | - | **transient** | `sleep` |
| 2503437 | `S,SN` | - | **transient** | `sleep` |
| 2503701 | `S,SN` | - | **transient** | `sleep` |
| 2503703 | `S,SN` | - | **transient** | `sleep` |
| 2503709 | `S,SN` | - | **transient** | `sleep` |
| 2503712 | `S,SN` | - | **transient** | `sleep` |
| 2503715 | `S,SN` | - | **transient** | `sleep` |
| 2503719 | `S,SN` | - | **transient** | `sleep` |
| 2504023 | `S,SN` | - | **transient** | `sleep` |
| 2504368 | `S,SN` | - | **transient** | `sleep` |
| 2504386 | `S,SN` | - | **transient** | `sleep` |
| 2504402 | `S,SN` | - | **transient** | `sleep` |
| 2504405 | `S,SN` | - | **transient** | `sleep` |
| 2504416 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.34 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.11 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `Rsl,S,Ssl` | 0.12 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-ssn-r3.log` / `leg1-ssn-r3`  (4 snapshots)

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
| 50122 | `S,Sl` | 1.02 | both ends | `kwin_wayland` |
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
| 583887 | `S,SNs` | 0.00 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.41 | both ends | `claude` |
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
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2491874 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491877 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2491878 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2501734 | `S,SN` | - | **transient** | `sleep` |
| 2503709 | `S,SN` | - | **transient** | `sleep` |
| 2503712 | `S,SN` | - | **transient** | `sleep` |
| 2503715 | `S,SN` | - | **transient** | `sleep` |
| 2503719 | `S,SN` | - | **transient** | `sleep` |
| 2504023 | `S,SN` | - | **transient** | `sleep` |
| 2504368 | `S,SN` | - | **transient** | `sleep` |
| 2504386 | `S,SN` | - | **transient** | `sleep` |
| 2504402 | `S,SN` | - | **transient** | `sleep` |
| 2504405 | `S,SN` | - | **transient** | `sleep` |
| 2504416 | `S,SN` | - | **transient** | `sleep` |
| 2506341 | `S,SN` | - | **transient** | `sleep` |
| 2506346 | `S,SN` | - | **transient** | `sleep` |
| 2506352 | `S,SN` | - | **transient** | `sleep` |
| 2506355 | `S,SN` | - | **transient** | `sleep` |
| 2506358 | `S,SN` | - | **transient** | `sleep` |
| 2506363 | `S,SN` | - | **transient** | `sleep` |
| 2506366 | `S,SN` | - | **transient** | `sleep` |
| 2507037 | `S,SN` | - | **transient** | `sleep` |
| 2507044 | `S,SN` | - | **transient** | `sleep` |
| 2507052 | `S,SN` | - | **transient** | `sleep` |
| 2507053 | `S,SN` | - | **transient** | `sleep` |
| 2507057 | `S,SN` | - | **transient** | `sleep` |
| 2507060 | `S,SN` | - | **transient** | `sleep` |
| 2507063 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.05 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.05 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.12 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.12 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-walk-r1.log` / `leg1-walk-r1`  (4 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.04 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.02 | both ends | `irqbalance` |
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
| 1218 | `S,Ss` | 0.02 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.07 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.03 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
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
| 1967 | `S,S<sl` | 0.01 | both ends | `wireplumber` |
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
| 4094 | `S,Ss` | 0.04 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 11.74 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.02 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.01 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.01 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.05 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.01 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.15 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.06 | both ends | `org_kde_powerde` |
| 50724 | `S,Ssl` | 0.01 | both ends | `xembedsniproxy` |
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
| 59463 | `S,Sl` | 0.02 | both ends | `kitten` |
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
| 116643 | `S,Sl` | 0.42 | both ends | `kscreenlocker_g` |
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
| 583906 | `S,SNs` | 0.02 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 3.58 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.11 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.12 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2491874 | `S` | - | **transient** | `systemd-userwor` |
| 2491878 | `S` | - | **transient** | `systemd-userwor` |
| 2506352 | `S,SN` | - | **transient** | `sleep` |
| 2506366 | `SN` | - | **transient** | `sleep` |
| 2507037 | `S,SN` | - | **transient** | `sleep` |
| 2507044 | `S,SN` | - | **transient** | `sleep` |
| 2507052 | `S,SN` | - | **transient** | `sleep` |
| 2507053 | `S,SN` | - | **transient** | `sleep` |
| 2507057 | `S,SN` | - | **transient** | `sleep` |
| 2507060 | `S,SN` | - | **transient** | `sleep` |
| 2507063 | `S,SN` | - | **transient** | `sleep` |
| 2508217 | `S,SN` | - | **transient** | `sleep` |
| 2508325 | `S,SN` | - | **transient** | `sleep` |
| 2508326 | `S` | - | **transient** | `systemd-userwor` |
| 2508985 | `S` | - | **transient** | `systemd-userwor` |
| 2508990 | `S` | - | **transient** | `systemd-userwor` |
| 2509259 | `S,SN` | - | **transient** | `sleep` |
| 2509268 | `S,SN` | - | **transient** | `sleep` |
| 2509270 | `S,SN` | - | **transient** | `sleep` |
| 2509289 | `S,SN` | - | **transient** | `sleep` |
| 2509299 | `S,SN` | - | **transient** | `sleep` |
| 2509301 | `S,SN` | - | **transient** | `sleep` |
| 2509305 | `S,SN` | - | **transient** | `sleep` |
| 2509307 | `S,SN` | - | **transient** | `sleep` |
| 2509310 | `S,SN` | - | **transient** | `sleep` |
| 2509312 | `S,SN` | - | **transient** | `sleep` |
| 2509315 | `S,SN` | - | **transient** | `sleep` |
| 2510110 | `S` | - | **transient** | `systemd-userwor` |
| 2510113 | `S` | - | **transient** | `systemd-userwor` |
| 2510114 | `S` | - | **transient** | `systemd-userwor` |
| 2510245 | `S,SN` | - | **transient** | `sleep` |
| 2510249 | `S,SN` | - | **transient** | `sleep` |
| 2510254 | `S,SN` | - | **transient** | `sleep` |
| 2510256 | `S,SN` | - | **transient** | `sleep` |
| 2510275 | `S,SN` | - | **transient** | `sleep` |
| 2510279 | `S,SN` | - | **transient** | `sleep` |
| 2510281 | `S,SN` | - | **transient** | `sleep` |
| 2510283 | `S,SN` | - | **transient** | `sleep` |
| 2510285 | `S,SN` | - | **transient** | `sleep` |
| 2510287 | `S,SN` | - | **transient** | `sleep` |
| 2510289 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.98 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.45 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.63 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.50 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.23 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.19 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.70 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.61 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.62 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 1.04 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.09 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.12 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-walk-r2.log` / `leg1-walk-r2`  (4 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.02 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.03 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.02 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.04 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.02 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.05 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.01 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.01 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.16 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.02 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.03 | both ends | `rsyslogd` |
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
| 4094 | `S,Ss` | 0.08 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 10.83 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.02 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.01 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.18 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.01 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.07 | both ends | `org_kde_powerde` |
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
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.51 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 3.78 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.13 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.13 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2616185 | `S` | - | **transient** | `systemd-userwor` |
| 2616217 | `S` | - | **transient** | `systemd-userwor` |
| 2616219 | `S` | - | **transient** | `systemd-userwor` |
| 2621559 | `S,SN` | - | **transient** | `sleep` |
| 2621572 | `SN` | - | **transient** | `sleep` |
| 2621581 | `S,SN` | - | **transient** | `sleep` |
| 2621596 | `S,SN` | - | **transient** | `sleep` |
| 2621603 | `S,SN` | - | **transient** | `sleep` |
| 2621608 | `S,SN` | - | **transient** | `sleep` |
| 2621610 | `S,SN` | - | **transient** | `sleep` |
| 2621612 | `S,SN` | - | **transient** | `sleep` |
| 2621615 | `S,SN` | - | **transient** | `sleep` |
| 2621617 | `S,SN` | - | **transient** | `sleep` |
| 2621619 | `S,SN` | - | **transient** | `sleep` |
| 2621621 | `S,SN` | - | **transient** | `sleep` |
| 2621622 | `S,SN` | - | **transient** | `sleep` |
| 2621624 | `S,SN` | - | **transient** | `sleep` |
| 2622320 | `S,SNs` | - | **transient** | `bash` |
| 2622485 | `S` | - | **transient** | `systemd-userwor` |
| 2622486 | `S` | - | **transient** | `systemd-userwor` |
| 2622487 | `S` | - | **transient** | `systemd-userwor` |
| 2622647 | `S,SN` | - | **transient** | `sleep` |
| 2622656 | `S,SN` | - | **transient** | `sleep` |
| 2622667 | `S,SN` | - | **transient** | `sleep` |
| 2622670 | `S,SN` | - | **transient** | `sleep` |
| 2622679 | `S,SN` | - | **transient** | `sleep` |
| 2622683 | `S,SN` | - | **transient** | `sleep` |
| 2622685 | `S,SN` | - | **transient** | `sleep` |
| 2622687 | `S,SN` | - | **transient** | `sleep` |
| 2622689 | `S,SN` | - | **transient** | `sleep` |
| 2622691 | `S,SN` | - | **transient** | `sleep` |
| 2622692 | `S,SN` | - | **transient** | `sleep` |
| 2622694 | `S,SN` | - | **transient** | `sleep` |
| 2622696 | `S,SN` | - | **transient** | `sleep` |
| 2622698 | `S,SN` | - | **transient** | `sleep` |
| 2622700 | `S,SN` | - | **transient** | `sleep` |
| 2623894 | `S,SN` | - | **transient** | `sleep` |
| 2623924 | `S,SN` | - | **transient** | `sleep` |
| 2623926 | `S,SN` | - | **transient** | `sleep` |
| 2623932 | `S,SN` | - | **transient** | `sleep` |
| 2623941 | `S,SN` | - | **transient** | `sleep` |
| 2623943 | `S,SN` | - | **transient** | `sleep` |
| 2623952 | `S,SN` | - | **transient** | `sleep` |
| 2623955 | `S,SN` | - | **transient** | `sleep` |
| 2623963 | `S,SN` | - | **transient** | `sleep` |
| 2623964 | `S,SN` | - | **transient** | `sleep` |
| 2623966 | `S,SN` | - | **transient** | `sleep` |
| 2623968 | `S,SN` | - | **transient** | `sleep` |
| 2623971 | `S,SN` | - | **transient** | `sleep` |
| 2623975 | `S,SN` | - | **transient** | `sleep` |
| 2623976 | `S` | - | **transient** | `systemd-userwor` |
| 2623979 | `S` | - | **transient** | `systemd-userwor` |
| 2623980 | `S` | - | **transient** | `systemd-userwor` |
| 2623982 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 2.21 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.60 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.67 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `R,S,Sl` | 0.53 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.26 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.25 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.55 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.02 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.67 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.67 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.92 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.12 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.13 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L4-leg1-walk-r3.log` / `leg1-walk-r3`  (4 snapshots)

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
| 968 | `S,Ss` | 0.02 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.03 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.02 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.01 | both ends | `polkitd` |
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
| 1218 | `S,Ss` | 0.02 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.08 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.02 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.01 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.01 | both ends | `wireplumber` |
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
| 4094 | `S,Ss` | 0.05 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 11.71 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.02 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.14 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.01 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.07 | both ends | `org_kde_powerde` |
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
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.44 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 3.45 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.13 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.13 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512494 | `S` | - | **transient** | `systemd-userwor` |
| 2512495 | `S` | - | **transient** | `systemd-userwor` |
| 2512496 | `S` | - | **transient** | `systemd-userwor` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513318 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513688 | `S,SNs` | - | **transient** | `bash` |
| 2513690 | `S,SN` | - | **transient** | `sleep` |
| 2513698 | `S,SN` | - | **transient** | `sleep` |
| 2515075 | `S,SN` | - | **transient** | `sleep` |
| 2515079 | `S,SN` | - | **transient** | `sleep` |
| 2515081 | `S,SN` | - | **transient** | `sleep` |
| 2515085 | `S,SN` | - | **transient** | `sleep` |
| 2515089 | `S,SN` | - | **transient** | `sleep` |
| 2515093 | `S,SN` | - | **transient** | `sleep` |
| 2515097 | `S,SN` | - | **transient** | `sleep` |
| 2515100 | `S,SN` | - | **transient** | `sleep` |
| 2515107 | `S,SN` | - | **transient** | `sleep` |
| 2515109 | `S,SN` | - | **transient** | `sleep` |
| 2515110 | `S,SN` | - | **transient** | `sleep` |
| 2515112 | `S,SN` | - | **transient** | `sleep` |
| 2515114 | `S,SN` | - | **transient** | `sleep` |
| 2515843 | `S` | - | **transient** | `systemd-userwor` |
| 2515844 | `S` | - | **transient** | `systemd-userwor` |
| 2515871 | `S` | - | **transient** | `systemd-userwor` |
| 2516120 | `S,SN` | - | **transient** | `sleep` |
| 2516160 | `S,SN` | - | **transient** | `sleep` |
| 2516166 | `S,SN` | - | **transient** | `sleep` |
| 2516169 | `S,SN` | - | **transient** | `sleep` |
| 2516171 | `S,SN` | - | **transient** | `sleep` |
| 2516173 | `S,SN` | - | **transient** | `sleep` |
| 2516176 | `S,SN` | - | **transient** | `sleep` |
| 2516177 | `SN` | - | **transient** | `sleep` |
| 2516179 | `S,SN` | - | **transient** | `sleep` |
| 2516181 | `S,SN` | - | **transient** | `sleep` |
| 2516183 | `S,SN` | - | **transient** | `sleep` |
| 2516185 | `S,SN` | - | **transient** | `sleep` |
| 2516188 | `S,SN` | - | **transient** | `sleep` |
| 2516204 | `S,SN` | - | **transient** | `sleep` |
| 2516207 | `S,SN` | - | **transient** | `sleep` |
| 2517107 | `S` | - | **transient** | `systemd-userwor` |
| 2517109 | `S` | - | **transient** | `systemd-userwor` |
| 2517131 | `S` | - | **transient** | `systemd-userwor` |
| 2517196 | `S,SN` | - | **transient** | `sleep` |
| 2517241 | `S,SN` | - | **transient** | `sleep` |
| 2517249 | `S,SN` | - | **transient** | `sleep` |
| 2517254 | `S,SN` | - | **transient** | `sleep` |
| 2517256 | `S,SN` | - | **transient** | `sleep` |
| 2517258 | `S,SN` | - | **transient** | `sleep` |
| 2517260 | `S,SN` | - | **transient** | `sleep` |
| 2517262 | `S,SN` | - | **transient** | `sleep` |
| 2517265 | `S,SN` | - | **transient** | `sleep` |
| 2517266 | `S,SN` | - | **transient** | `sleep` |
| 2517268 | `S,SN` | - | **transient** | `sleep` |
| 2517270 | `S,SN` | - | **transient** | `sleep` |
| 2517272 | `S,SN` | - | **transient** | `sleep` |
| 2517275 | `S,SN` | - | **transient** | `sleep` |
| 2517293 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 2.03 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.56 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.63 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.50 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.25 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.18 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.51 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.62 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.63 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.87 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.13 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.15 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L5-leg1perf-ipm.log` / `leg1perf-ipm`  (8 snapshots)

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
| 1097257 | `S,Sl+` | 0.36 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.14 | both ends | `codex` |
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
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513318 | `S,SNs` | 0.00 | both ends | `bash` |
| 2517107 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2517109 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2517131 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2517254 | `S,SN` | - | **transient** | `sleep` |
| 2517258 | `S,SN` | - | **transient** | `sleep` |
| 2517260 | `S,SN` | 0.00 | both ends | `sleep` |
| 2517265 | `S,SN` | - | **transient** | `sleep` |
| 2517268 | `S,SN` | - | **transient** | `sleep` |
| 2517272 | `S,SN` | - | **transient** | `sleep` |
| 2517275 | `S,SN` | - | **transient** | `sleep` |
| 2517293 | `S,SN` | - | **transient** | `sleep` |
| 2518602 | `S,SN` | - | **transient** | `sleep` |
| 2518604 | `S,SN` | - | **transient** | `sleep` |
| 2518605 | `S,SN` | - | **transient** | `sleep` |
| 2518607 | `S,SN` | - | **transient** | `sleep` |
| 2518609 | `S,SN` | - | **transient** | `sleep` |
| 2518611 | `S,SN` | - | **transient** | `sleep` |
| 2518612 | `S,SN` | 0.00 | both ends | `sleep` |
| 2519301 | `S,SN` | - | **transient** | `sleep` |
| 2519314 | `S,SN` | - | **transient** | `sleep` |
| 2519338 | `S,SN` | - | **transient** | `sleep` |
| 2519742 | `S,SN` | - | **transient** | `sleep` |
| 2520032 | `S,SN` | - | **transient** | `sleep` |
| 2520056 | `S,SN` | - | **transient** | `sleep` |
| 2520069 | `S,SN` | - | **transient** | `sleep` |
| 2520071 | `S,SN` | - | **transient** | `sleep` |
| 2520756 | `S,SN` | - | **transient** | `sleep` |
| 2520792 | `S,SN` | - | **transient** | `sleep` |
| 2521501 | `S,SN` | - | **transient** | `sleep` |
| 2521526 | `S,SN` | - | **transient** | `sleep` |
| 2522198 | `S,SN` | - | **transient** | `sleep` |
| 2522213 | `S,SN` | - | **transient** | `sleep` |
| 2522215 | `S,SN` | - | **transient** | `sleep` |
| 2522251 | `S,SN` | - | **transient** | `sleep` |
| 2522968 | `S,SN` | - | **transient** | `sleep` |
| 2522981 | `RN` | - | **transient** | `pgrep` |
| 2522994 | `S,SN` | - | **transient** | `sleep` |
| 2523248 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.26 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.02 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.09 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L5-leg1perf-ssn.log` / `leg1perf-ssn`  (8 snapshots)

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
| 50122 | `S,Sl` | 0.79 | both ends | `kwin_wayland` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.27 | both ends | `claude` |
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
| 2470969 | `S,SNs` | 0.01 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513318 | `S,SNs` | 0.01 | both ends | `bash` |
| 2517107 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2517109 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2517131 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2518612 | `S,SN` | - | **transient** | `sleep` |
| 2519742 | `S,SN` | - | **transient** | `sleep` |
| 2520056 | `S,SN` | - | **transient** | `sleep` |
| 2520071 | `S,SN` | - | **transient** | `sleep` |
| 2520792 | `S,SN` | - | **transient** | `sleep` |
| 2521501 | `S,SN` | - | **transient** | `sleep` |
| 2521526 | `S,SN` | - | **transient** | `sleep` |
| 2522198 | `S,SN` | - | **transient** | `sleep` |
| 2522213 | `S,SN` | - | **transient** | `sleep` |
| 2522215 | `S,SN` | - | **transient** | `sleep` |
| 2522251 | `S,SN` | - | **transient** | `sleep` |
| 2522968 | `S,SN` | - | **transient** | `sleep` |
| 2522994 | `S,SN` | - | **transient** | `sleep` |
| 2523248 | `S,SN` | 0.00 | both ends | `sleep` |
| 2524190 | `S,SN` | 0.00 | both ends | `sleep` |
| 2524978 | `S,SN` | - | **transient** | `sleep` |
| 2525002 | `S,SN` | - | **transient** | `sleep` |
| 2525015 | `S,SN` | - | **transient** | `sleep` |
| 2525699 | `S,SN` | - | **transient** | `sleep` |
| 2525702 | `S,SN` | - | **transient** | `sleep` |
| 2525703 | `S,SN` | - | **transient** | `sleep` |
| 2526410 | `S,SN` | - | **transient** | `sleep` |
| 2527155 | `S,SN` | - | **transient** | `sleep` |
| 2527190 | `S,SN` | - | **transient** | `sleep` |
| 2527424 | `S,SN` | - | **transient** | `sleep` |
| 2527749 | `S,SN` | - | **transient** | `sleep` |
| 2527874 | `S,SN` | - | **transient** | `sleep` |
| 2527909 | `S,SN` | - | **transient** | `sleep` |
| 2527911 | `S,SN` | - | **transient** | `sleep` |
| 2528594 | `S,SN` | - | **transient** | `sleep` |
| 2528629 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.02 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.09 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.00 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L5-leg1perf-walk.log` / `leg1perf-walk`  (8 snapshots)

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
| 50122 | `S,Sl` | 0.45 | both ends | `kwin_wayland` |
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
| 1097257 | `S,Sl+` | 0.18 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.10 | both ends | `codex` |
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
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513318 | `S,SNs` | 0.00 | both ends | `bash` |
| 2517107 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2517109 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2517131 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2524190 | `S,SN` | 0.00 | both ends | `sleep` |
| 2525002 | `S,SN` | - | **transient** | `sleep` |
| 2525699 | `S,SN` | - | **transient** | `sleep` |
| 2525703 | `S,SN` | - | **transient** | `sleep` |
| 2526410 | `S,SN` | - | **transient** | `sleep` |
| 2527155 | `S,SN` | 0.00 | both ends | `sleep` |
| 2527190 | `S,SN` | 0.00 | both ends | `sleep` |
| 2527424 | `S,SN` | - | **transient** | `sleep` |
| 2527749 | `S,SN` | - | **transient** | `sleep` |
| 2527874 | `S,SN` | 0.00 | both ends | `sleep` |
| 2527909 | `S,SN` | - | **transient** | `sleep` |
| 2527911 | `S,SN` | - | **transient** | `sleep` |
| 2528594 | `S,SN` | 0.00 | both ends | `sleep` |
| 2528629 | `S,SN` | 0.00 | both ends | `sleep` |
| 2529918 | `S,SN` | 0.00 | both ends | `sleep` |
| 2530633 | `S,SN` | - | **transient** | `sleep` |
| 2531350 | `S,SN` | - | **transient** | `sleep` |
| 2531384 | `S,SN` | - | **transient** | `sleep` |
| 2532080 | `S,SN` | - | **transient** | `sleep` |
| 2532484 | `S,SN` | - | **transient** | `sleep` |
| 2532500 | `S,SN` | - | **transient** | `sleep` |
| 2532817 | `S,SN` | - | **transient** | `sleep` |
| 2534206 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.01 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.01 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.01 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.04 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ipm-off-passA.log` / `leg2-ipm-off-passA`  (5 snapshots)

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
| 1240 | `S,Ssl` | 0.01 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.04 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.01 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.02 | both ends | `tuned-ppd` |
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
| 50122 | `S,Sl` | 6.12 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.05 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.01 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.07 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.01 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.05 | both ends | `org_kde_powerde` |
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
| 116643 | `S,Sl` | 0.23 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 1.83 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.10 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.08 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.02 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2544996 | `S` | - | **transient** | `systemd-userwor` |
| 2544999 | `S` | - | **transient** | `systemd-userwor` |
| 2545021 | `S` | - | **transient** | `systemd-userwor` |
| 2547830 | `S,SN` | - | **transient** | `sleep` |
| 2548525 | `SN` | - | **transient** | `sleep` |
| 2548528 | `S,SN` | - | **transient** | `sleep` |
| 2548545 | `S,SN` | - | **transient** | `sleep` |
| 2548547 | `S,SN` | - | **transient** | `sleep` |
| 2548549 | `S,SN` | - | **transient** | `sleep` |
| 2548551 | `S,SN` | - | **transient** | `sleep` |
| 2548556 | `S,SN` | - | **transient** | `sleep` |
| 2548557 | `S,SN` | - | **transient** | `sleep` |
| 2548558 | `S,SN` | - | **transient** | `sleep` |
| 2548562 | `S,SN` | - | **transient** | `sleep` |
| 2548564 | `S,SN` | - | **transient** | `sleep` |
| 2548567 | `S,SN` | - | **transient** | `sleep` |
| 2548588 | `S,SN` | - | **transient** | `sleep` |
| 2550628 | `S,SN` | - | **transient** | `sleep` |
| 2550638 | `S,SN` | - | **transient** | `sleep` |
| 2550642 | `S,SN` | - | **transient** | `sleep` |
| 2550663 | `S,SN` | - | **transient** | `sleep` |
| 2550665 | `S,SN` | - | **transient** | `sleep` |
| 2550667 | `S,SN` | - | **transient** | `sleep` |
| 2550669 | `S,SN` | - | **transient** | `sleep` |
| 2550671 | `S,SN` | - | **transient** | `sleep` |
| 2550673 | `S,SN` | - | **transient** | `sleep` |
| 2550674 | `S,SN` | - | **transient** | `sleep` |
| 2550676 | `S,SN` | - | **transient** | `sleep` |
| 2550678 | `S,SN` | - | **transient** | `sleep` |
| 2550680 | `S,SN` | - | **transient** | `sleep` |
| 2550682 | `S,SN` | - | **transient** | `sleep` |
| 2551382 | `S,SN` | - | **transient** | `sleep` |
| 2551445 | `S,SN` | - | **transient** | `sleep` |
| 2551448 | `S,SN` | - | **transient** | `sleep` |
| 2551450 | `S,SN` | - | **transient** | `sleep` |
| 2551452 | `S,SN` | - | **transient** | `sleep` |
| 2551454 | `S,SN` | - | **transient** | `sleep` |
| 2551456 | `S,SN` | - | **transient** | `sleep` |
| 2551459 | `S,SN` | - | **transient** | `sleep` |
| 2551462 | `S,SN` | - | **transient** | `sleep` |
| 2551464 | `S,SN` | - | **transient** | `sleep` |
| 2551469 | `S,SN` | - | **transient** | `sleep` |
| 2551470 | `S` | - | **transient** | `systemd-userwor` |
| 2551478 | `S` | - | **transient** | `systemd-userwor` |
| 2551495 | `S,SN` | - | **transient** | `sleep` |
| 2551497 | `S,SN` | - | **transient** | `sleep` |
| 2551498 | `S,SN` | - | **transient** | `sleep` |
| 2552156 | `S` | - | **transient** | `systemd-userwor` |
| 2552209 | `S,SN` | - | **transient** | `sleep` |
| 2552232 | `S,SN` | - | **transient** | `sleep` |
| 2552234 | `S,SN` | - | **transient** | `sleep` |
| 2552258 | `S,SN` | - | **transient** | `sleep` |
| 2552260 | `S,SN` | - | **transient** | `sleep` |
| 2552264 | `S,SN` | - | **transient** | `sleep` |
| 2552266 | `S,SN` | - | **transient** | `sleep` |
| 2552269 | `S,SN` | - | **transient** | `sleep` |
| 2552271 | `S,SN` | - | **transient** | `sleep` |
| 2552273 | `S,SN` | - | **transient** | `sleep` |
| 2552275 | `S,SN` | - | **transient** | `sleep` |
| 2552278 | `S,SN` | - | **transient** | `sleep` |
| 2552280 | `S,SN` | - | **transient** | `sleep` |
| 2552281 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.92 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.29 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.35 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.27 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.10 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.81 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.35 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.35 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.40 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.06 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.07 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ipm-off-passB.log` / `leg2-ipm-off-passB`  (5 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.03 | both ends | `NetworkManager` |
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
| 1155 | `S,Ss` | 0.01 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.02 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.04 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
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
| 4094 | `S,Ss` | 0.04 | both ends | `tmux:` |
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
| 50122 | `R,S,Sl` | 6.55 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `R,S,Ssl` | 0.09 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.04 | both ends | `org_kde_powerde` |
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
| 59463 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 116643 | `S,Sl` | 0.26 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 2.02 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.12 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.10 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2551470 | `S` | - | **transient** | `systemd-userwor` |
| 2551478 | `S` | - | **transient** | `systemd-userwor` |
| 2552156 | `S` | - | **transient** | `systemd-userwor` |
| 2552209 | `S,SN` | - | **transient** | `sleep` |
| 2552232 | `SN` | - | **transient** | `sleep` |
| 2552234 | `S,SN` | - | **transient** | `sleep` |
| 2552258 | `S,SN` | - | **transient** | `sleep` |
| 2552260 | `S,SN` | - | **transient** | `sleep` |
| 2552264 | `S,SN` | - | **transient** | `sleep` |
| 2552266 | `S,SN` | - | **transient** | `sleep` |
| 2552269 | `S,SN` | - | **transient** | `sleep` |
| 2552271 | `S,SN` | - | **transient** | `sleep` |
| 2552273 | `S,SN` | - | **transient** | `sleep` |
| 2552275 | `S,SN` | - | **transient** | `sleep` |
| 2552278 | `S,SN` | - | **transient** | `sleep` |
| 2552280 | `S,SN` | - | **transient** | `sleep` |
| 2552281 | `S,SN` | - | **transient** | `sleep` |
| 2554276 | `S,SN` | - | **transient** | `sleep` |
| 2554336 | `S,SN` | - | **transient** | `sleep` |
| 2554338 | `S,SN` | - | **transient** | `sleep` |
| 2554340 | `S,SN` | - | **transient** | `sleep` |
| 2554353 | `S,SN` | - | **transient** | `sleep` |
| 2554355 | `S,SN` | - | **transient** | `sleep` |
| 2554358 | `S,SN` | - | **transient** | `sleep` |
| 2554360 | `S,SN` | - | **transient** | `sleep` |
| 2554363 | `S,SN` | - | **transient** | `sleep` |
| 2554380 | `S,SN` | - | **transient** | `sleep` |
| 2554384 | `S,SN` | - | **transient** | `sleep` |
| 2554385 | `S,SN` | - | **transient** | `sleep` |
| 2554395 | `S,SN` | - | **transient** | `sleep` |
| 2554397 | `S,SN` | - | **transient** | `sleep` |
| 2555079 | `S,SNs` | - | **transient** | `bash` |
| 2555118 | `S,SN` | - | **transient** | `sleep` |
| 2555166 | `S,SN` | - | **transient** | `sleep` |
| 2555168 | `S,SN` | - | **transient** | `sleep` |
| 2555179 | `S,SN` | - | **transient** | `sleep` |
| 2555184 | `S,SN` | - | **transient** | `sleep` |
| 2555185 | `S,SN` | - | **transient** | `sleep` |
| 2555187 | `S,SN` | - | **transient** | `sleep` |
| 2555189 | `S,SN` | - | **transient** | `sleep` |
| 2555191 | `S,SN` | - | **transient** | `sleep` |
| 2555193 | `S,SN` | - | **transient** | `sleep` |
| 2555194 | `S,SN` | - | **transient** | `sleep` |
| 2555196 | `S,SN` | - | **transient** | `sleep` |
| 2555198 | `S,SN` | - | **transient** | `sleep` |
| 2555204 | `S,SN` | - | **transient** | `sleep` |
| 2555207 | `S,SN` | - | **transient** | `sleep` |
| 2555988 | `S,SN` | - | **transient** | `sleep` |
| 2556003 | `S,SNs` | - | **transient** | `bash` |
| 2556018 | `S,SN` | - | **transient** | `sleep` |
| 2556020 | `S,SN` | - | **transient** | `sleep` |
| 2556043 | `S` | - | **transient** | `systemd-userwor` |
| 2556044 | `S` | - | **transient** | `systemd-userwor` |
| 2556047 | `S,SN` | - | **transient** | `sleep` |
| 2556050 | `S,SN` | - | **transient** | `sleep` |
| 2556051 | `SN` | - | **transient** | `sleep` |
| 2556053 | `S,SN` | - | **transient** | `sleep` |
| 2556054 | `S` | - | **transient** | `systemd-userwor` |
| 2556056 | `S,SN` | - | **transient** | `sleep` |
| 2556060 | `S,SN` | - | **transient** | `sleep` |
| 2556061 | `S,SNs` | - | **transient** | `bash` |
| 2556064 | `S,SN` | - | **transient** | `flock` |
| 2556066 | `S,SN` | - | **transient** | `sleep` |
| 2556068 | `S,SN` | - | **transient** | `sleep` |
| 2556070 | `S,SN` | - | **transient** | `sleep` |
| 2556072 | `S,SN` | - | **transient** | `sleep` |
| 2556082 | `S,SN` | - | **transient** | `sleep` |
| 2556085 | `S,SN` | - | **transient** | `sleep` |
| 2556098 | `S,SN` | - | **transient** | `sleep` |
| 2556207 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.16 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.67 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.36 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.28 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.12 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.88 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.02 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.37 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `Rl,S,Sl` | 0.37 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.46 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.06 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.07 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ipm-sink-passA.log` / `leg2-ipm-sink-passA`  (5 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.02 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.01 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.03 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.01 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.01 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.01 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.01 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.00 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.01 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.01 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.26 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.02 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.01 | both ends | `dbus-broker` |
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
| 4094 | `S,Ss` | 0.08 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 6.07 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.05 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.09 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.01 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.03 | both ends | `org_kde_powerde` |
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
| 116643 | `S,Sl` | 0.27 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 2.00 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.12 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.11 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2555079 | `S,SNs` | 0.00 | both ends | `bash` |
| 2555988 | `S,SN` | - | **transient** | `sleep` |
| 2556003 | `S,SNs` | 0.00 | both ends | `bash` |
| 2556018 | `S,SN` | - | **transient** | `sleep` |
| 2556020 | `S,SN` | - | **transient** | `sleep` |
| 2556043 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2556044 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2556047 | `S,SN` | - | **transient** | `sleep` |
| 2556050 | `S,SN` | - | **transient** | `sleep` |
| 2556053 | `S,SN` | - | **transient** | `sleep` |
| 2556054 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2556056 | `S,SN` | - | **transient** | `sleep` |
| 2556060 | `S,SN` | - | **transient** | `sleep` |
| 2556066 | `S,SN` | - | **transient** | `sleep` |
| 2556068 | `S,SN` | - | **transient** | `sleep` |
| 2556070 | `S,SN` | - | **transient** | `sleep` |
| 2556072 | `S,SN` | - | **transient** | `sleep` |
| 2556082 | `S,SN` | - | **transient** | `sleep` |
| 2556085 | `S,SN` | - | **transient** | `sleep` |
| 2556098 | `S,SN` | - | **transient** | `sleep` |
| 2556207 | `S,SN` | - | **transient** | `sleep` |
| 2558190 | `S,SN` | - | **transient** | `sleep` |
| 2558299 | `S,SNs` | - | **transient** | `bash` |
| 2558386 | `S,Ssl` | - | **transient** | `tailscaled` |
| 2558394 | `S` | - | **transient** | `fish` |
| 2558490 | `S,Ss` | - | **transient** | `systemd-hostnam` |
| 2558537 | `S,S+` | - | **transient** | `tmux:` |
| 2558543 | `S,SN` | - | **transient** | `sleep` |
| 2558545 | `S,SN` | - | **transient** | `sleep` |
| 2558547 | `S,SN` | - | **transient** | `sleep` |
| 2558553 | `S,SN` | - | **transient** | `sleep` |
| 2558556 | `S,SN` | - | **transient** | `sleep` |
| 2558558 | `S,SN` | - | **transient** | `sleep` |
| 2558567 | `S,SN` | - | **transient** | `sleep` |
| 2558569 | `S,SN` | - | **transient** | `sleep` |
| 2558571 | `S,SN` | - | **transient** | `sleep` |
| 2558576 | `S,SN` | - | **transient** | `sleep` |
| 2558579 | `S,SN` | - | **transient** | `sleep` |
| 2558596 | `S,SN` | - | **transient** | `sleep` |
| 2558598 | `S,SN` | - | **transient** | `sleep` |
| 2558600 | `S,SN` | - | **transient** | `sleep` |
| 2558602 | `S,SN` | - | **transient** | `sleep` |
| 2558604 | `S,SN` | - | **transient** | `sleep` |
| 2559389 | `S,SN` | - | **transient** | `sleep` |
| 2559406 | `SN` | - | **transient** | `sleep` |
| 2559408 | `S,SN` | - | **transient** | `sleep` |
| 2559430 | `S,SN` | - | **transient** | `sleep` |
| 2559432 | `S,SN` | - | **transient** | `sleep` |
| 2559435 | `S,SN` | - | **transient** | `sleep` |
| 2559437 | `S,SN` | - | **transient** | `sleep` |
| 2559440 | `S,SN` | - | **transient** | `sleep` |
| 2559444 | `S,SN` | - | **transient** | `sleep` |
| 2559446 | `S,SN` | - | **transient** | `sleep` |
| 2559447 | `S,SN` | - | **transient** | `sleep` |
| 2559451 | `S,SN` | - | **transient** | `sleep` |
| 2559453 | `S,SN` | - | **transient** | `sleep` |
| 2559455 | `S,SN` | - | **transient** | `sleep` |
| 2559457 | `S,SN` | - | **transient** | `sleep` |
| 2559458 | `S,SN` | - | **transient** | `sleep` |
| 2559464 | `S,SN` | - | **transient** | `sleep` |
| 2560194 | `S,SN` | - | **transient** | `sleep` |
| 2560271 | `S,SN` | - | **transient** | `sleep` |
| 2560275 | `S,SN` | - | **transient** | `sleep` |
| 2560277 | `S,SN` | - | **transient** | `sleep` |
| 2560281 | `S,SN` | - | **transient** | `sleep` |
| 2560285 | `S,SN` | - | **transient** | `sleep` |
| 2560287 | `S,SN` | - | **transient** | `sleep` |
| 2560289 | `S,SN` | - | **transient** | `sleep` |
| 2560293 | `S,SN` | - | **transient** | `sleep` |
| 2560295 | `S,SN` | - | **transient** | `sleep` |
| 2560298 | `S,SN` | - | **transient** | `sleep` |
| 2560301 | `S,SN` | - | **transient** | `sleep` |
| 2560318 | `S,SN` | - | **transient** | `sleep` |
| 2560345 | `S,SN` | - | **transient** | `sleep` |
| 2560346 | `S,SN` | - | **transient** | `sleep` |
| 2560348 | `S,SN` | - | **transient** | `sleep` |
| 2560350 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.18 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.31 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.64 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.29 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.14 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.09 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.05 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.35 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.37 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.57 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.08 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.09 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ipm-sink-passB.log` / `leg2-ipm-sink-passB`  (5 snapshots)

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
| 979 | `S,Ssl` | 0.02 | both ends | `irqbalance` |
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
| 1244 | `S,Ssl` | 0.05 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 6.39 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.09 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.05 | both ends | `org_kde_powerde` |
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
| 116643 | `S,Sl` | 0.25 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 2.04 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.10 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.10 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2555079 | `S,SNs` | 0.01 | both ends | `bash` |
| 2556003 | `S,SNs` | 0.01 | both ends | `bash` |
| 2556043 | `S` | - | **transient** | `systemd-userwor` |
| 2556044 | `S` | - | **transient** | `systemd-userwor` |
| 2556054 | `S` | - | **transient** | `systemd-userwor` |
| 2558299 | `S,SNs` | 0.00 | both ends | `bash` |
| 2560194 | `S,SN` | - | **transient** | `sleep` |
| 2560271 | `S,SN` | - | **transient** | `sleep` |
| 2560275 | `S,SN` | - | **transient** | `sleep` |
| 2560277 | `S,SN` | - | **transient** | `sleep` |
| 2560281 | `S,SN` | - | **transient** | `sleep` |
| 2560285 | `S,SN` | - | **transient** | `sleep` |
| 2560287 | `S,SN` | - | **transient** | `sleep` |
| 2560289 | `S,SN` | - | **transient** | `sleep` |
| 2560293 | `S,SN` | - | **transient** | `sleep` |
| 2560295 | `S,SN` | - | **transient** | `sleep` |
| 2560298 | `S,SN` | - | **transient** | `sleep` |
| 2560301 | `S,SN` | - | **transient** | `sleep` |
| 2560318 | `S,SN` | - | **transient** | `sleep` |
| 2560345 | `S,SN` | - | **transient** | `sleep` |
| 2560346 | `S,SN` | - | **transient** | `sleep` |
| 2560348 | `S,SN` | - | **transient** | `sleep` |
| 2560350 | `S,SN` | - | **transient** | `sleep` |
| 2562445 | `S,SN` | - | **transient** | `sleep` |
| 2562505 | `S,SN` | - | **transient** | `sleep` |
| 2562508 | `S,SN` | - | **transient** | `sleep` |
| 2562511 | `S,SN` | - | **transient** | `sleep` |
| 2562513 | `S,SN` | - | **transient** | `sleep` |
| 2562518 | `S,SN` | - | **transient** | `sleep` |
| 2562520 | `S,SN` | - | **transient** | `sleep` |
| 2562522 | `S,SN` | - | **transient** | `sleep` |
| 2562524 | `S,SN` | - | **transient** | `sleep` |
| 2562526 | `S,SN` | - | **transient** | `sleep` |
| 2562528 | `S,SN` | - | **transient** | `sleep` |
| 2562530 | `S,SN` | - | **transient** | `sleep` |
| 2562531 | `S,SN` | - | **transient** | `sleep` |
| 2562533 | `S,SN` | - | **transient** | `sleep` |
| 2562534 | `S` | - | **transient** | `systemd-userwor` |
| 2562535 | `S` | - | **transient** | `systemd-userwor` |
| 2562536 | `S` | - | **transient** | `systemd-userwor` |
| 2562539 | `S,SN` | - | **transient** | `sleep` |
| 2562541 | `S,SN` | - | **transient** | `sleep` |
| 2562544 | `S,SN` | - | **transient** | `sleep` |
| 2563325 | `S,SN` | - | **transient** | `sleep` |
| 2563327 | `S,SN` | - | **transient** | `sleep` |
| 2563329 | `S,SN` | - | **transient** | `sleep` |
| 2563344 | `S,SN` | - | **transient** | `sleep` |
| 2563350 | `S,SN` | - | **transient** | `sleep` |
| 2563352 | `S,SN` | - | **transient** | `sleep` |
| 2563355 | `S,SN` | - | **transient** | `sleep` |
| 2563359 | `S,SN` | - | **transient** | `sleep` |
| 2563361 | `S,SN` | - | **transient** | `sleep` |
| 2563378 | `S,SN` | - | **transient** | `sleep` |
| 2563380 | `S,SN` | - | **transient** | `sleep` |
| 2563381 | `S,SN` | - | **transient** | `sleep` |
| 2563383 | `S,SN` | - | **transient** | `sleep` |
| 2563393 | `S,SN` | - | **transient** | `sleep` |
| 2563395 | `S,SN` | - | **transient** | `sleep` |
| 2563399 | `S,SN` | - | **transient** | `sleep` |
| 2563400 | `S,SN` | - | **transient** | `sleep` |
| 2564144 | `S,SN` | - | **transient** | `sleep` |
| 2564193 | `S,SN` | - | **transient** | `sleep` |
| 2564195 | `S,SN` | - | **transient** | `sleep` |
| 2564198 | `S,SN` | - | **transient** | `sleep` |
| 2564206 | `S,SN` | - | **transient** | `sleep` |
| 2564207 | `S,SN` | - | **transient** | `sleep` |
| 2564209 | `S,SN` | - | **transient** | `sleep` |
| 2564214 | `S,SN` | - | **transient** | `sleep` |
| 2564216 | `S,SN` | - | **transient** | `sleep` |
| 2564218 | `S,SN` | - | **transient** | `sleep` |
| 2564219 | `S,SN` | - | **transient** | `sleep` |
| 2564221 | `S,SN` | - | **transient** | `sleep` |
| 2564224 | `S,SN` | - | **transient** | `sleep` |
| 2564226 | `S,SN` | - | **transient** | `sleep` |
| 2564228 | `S,SN` | - | **transient** | `sleep` |
| 2564230 | `S,SN` | - | **transient** | `sleep` |
| 2564233 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.15 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.30 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.35 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.24 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.86 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.02 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.37 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.34 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.58 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.06 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.08 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ssn-off-passA.log` / `leg2-ssn-off-passA`  (5 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.02 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.03 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.04 | both ends | `NetworkManager` |
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
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.07 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.02 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.03 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.10 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.01 | both ends | `dbus-broker` |
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
| 4094 | `S,Ss` | 0.07 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 10.22 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.01 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.06 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.16 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.01 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.06 | both ends | `org_kde_powerde` |
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
| 59463 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 116643 | `S,Sl` | 0.43 | both ends | `kscreenlocker_g` |
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
| 583906 | `S,SNs` | 0.01 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 3.38 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.14 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.13 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2555079 | `S,SNs` | 0.00 | both ends | `bash` |
| 2556003 | `S,SNs` | 0.00 | both ends | `bash` |
| 2558299 | `S,SNs` | 0.01 | both ends | `bash` |
| 2562534 | `S` | - | **transient** | `systemd-userwor` |
| 2562535 | `S` | - | **transient** | `systemd-userwor` |
| 2562536 | `S` | - | **transient** | `systemd-userwor` |
| 2564144 | `S,SN` | - | **transient** | `sleep` |
| 2564193 | `S,SN` | - | **transient** | `sleep` |
| 2564195 | `S,SN` | - | **transient** | `sleep` |
| 2564198 | `S,SN` | - | **transient** | `sleep` |
| 2564206 | `S,SN` | - | **transient** | `sleep` |
| 2564207 | `S,SN` | - | **transient** | `sleep` |
| 2564214 | `S,SN` | - | **transient** | `sleep` |
| 2564216 | `S,SN` | - | **transient** | `sleep` |
| 2564218 | `S,SN` | - | **transient** | `sleep` |
| 2564219 | `S,SN` | - | **transient** | `sleep` |
| 2564221 | `S,SN` | - | **transient** | `sleep` |
| 2564224 | `S,SN` | - | **transient** | `sleep` |
| 2564226 | `S,SN` | - | **transient** | `sleep` |
| 2564228 | `S,SN` | - | **transient** | `sleep` |
| 2564230 | `S,SN` | - | **transient** | `sleep` |
| 2564233 | `S,SN` | - | **transient** | `sleep` |
| 2565565 | `S,SN` | - | **transient** | `sleep` |
| 2566412 | `S,SN` | - | **transient** | `sleep` |
| 2566468 | `S,SN` | - | **transient** | `sleep` |
| 2566471 | `S,SN` | - | **transient** | `sleep` |
| 2566476 | `S,SN` | - | **transient** | `sleep` |
| 2566481 | `S,SN` | - | **transient** | `sleep` |
| 2566485 | `S,SN` | - | **transient** | `sleep` |
| 2566487 | `S,SN` | - | **transient** | `sleep` |
| 2566491 | `S,SN` | - | **transient** | `sleep` |
| 2566492 | `S,SN` | - | **transient** | `sleep` |
| 2566494 | `S,SN` | - | **transient** | `sleep` |
| 2566499 | `S,SN` | - | **transient** | `sleep` |
| 2566501 | `S,SN` | - | **transient** | `sleep` |
| 2566503 | `S,SN` | - | **transient** | `sleep` |
| 2566506 | `S,SN` | - | **transient** | `sleep` |
| 2566510 | `S,SN` | - | **transient** | `sleep` |
| 2566512 | `S,SN` | - | **transient** | `sleep` |
| 2566529 | `S,SN` | - | **transient** | `sleep` |
| 2567234 | `S` | - | **transient** | `systemd-userwor` |
| 2567238 | `S` | - | **transient** | `systemd-userwor` |
| 2567239 | `S` | - | **transient** | `systemd-userwor` |
| 2567372 | `S,SN` | - | **transient** | `sleep` |
| 2567424 | `S,SN` | - | **transient** | `sleep` |
| 2567432 | `S,SN` | - | **transient** | `sleep` |
| 2567434 | `S,SN` | - | **transient** | `sleep` |
| 2567436 | `S,SN` | - | **transient** | `sleep` |
| 2567439 | `S,SN` | - | **transient** | `sleep` |
| 2567443 | `S,SN` | - | **transient** | `sleep` |
| 2567448 | `S,SN` | - | **transient** | `sleep` |
| 2567450 | `S,SN` | - | **transient** | `sleep` |
| 2567452 | `S,SN` | - | **transient** | `sleep` |
| 2567455 | `S,SN` | - | **transient** | `sleep` |
| 2567457 | `S,SN` | - | **transient** | `sleep` |
| 2567459 | `S,SN` | - | **transient** | `sleep` |
| 2567477 | `S,SN` | - | **transient** | `sleep` |
| 2567478 | `S,SN` | - | **transient** | `sleep` |
| 2567480 | `S,SN` | - | **transient** | `sleep` |
| 2567483 | `S,SN` | - | **transient** | `sleep` |
| 2568309 | `S,SN` | - | **transient** | `sleep` |
| 2568360 | `S,SN` | - | **transient** | `sleep` |
| 2568365 | `S,SN` | - | **transient** | `sleep` |
| 2568367 | `S,SN` | - | **transient** | `sleep` |
| 2568372 | `S,SN` | - | **transient** | `sleep` |
| 2568377 | `S,SN` | - | **transient** | `sleep` |
| 2568379 | `S,SN` | - | **transient** | `sleep` |
| 2568381 | `S,SN` | - | **transient** | `sleep` |
| 2568389 | `S,SN` | - | **transient** | `sleep` |
| 2568392 | `S,SN` | - | **transient** | `sleep` |
| 2568394 | `S,SN` | - | **transient** | `sleep` |
| 2568396 | `S,SN` | - | **transient** | `sleep` |
| 2568413 | `S,SN` | - | **transient** | `sleep` |
| 2568414 | `S,SN` | - | **transient** | `sleep` |
| 2568416 | `S,SN` | - | **transient** | `sleep` |
| 2568418 | `S,SN` | - | **transient** | `sleep` |
| 2568422 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.73 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.44 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.61 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.49 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.22 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.38 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.57 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.61 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.97 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.12 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.12 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ssn-off-passB.log` / `leg2-ssn-off-passB`  (5 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.02 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.04 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.02 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.01 | both ends | `polkitd` |
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
| 1218 | `S,Ss` | 0.02 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.06 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.03 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.01 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.01 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.01 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.01 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.02 | both ends | `pipewire-pulse` |
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
| 4094 | `S,Ss` | 0.05 | both ends | `tmux:` |
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
| 50122 | `Rl,S,Sl` | 10.40 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.01 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.01 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.15 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.01 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.06 | both ends | `org_kde_powerde` |
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
| 59662 | `S,Sl` | 0.00 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.00 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.41 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 3.34 | both ends | `claude` |
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
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.02 | both ends | `bash` |
| 2555079 | `S,SNs` | 0.01 | both ends | `bash` |
| 2556003 | `S,SNs` | - | **transient** | `bash` |
| 2558299 | `S,SNs` | 0.02 | both ends | `bash` |
| 2567234 | `S` | - | **transient** | `systemd-userwor` |
| 2567238 | `S` | - | **transient** | `systemd-userwor` |
| 2567239 | `S` | - | **transient** | `systemd-userwor` |
| 2568309 | `S,SN` | - | **transient** | `sleep` |
| 2568360 | `S,SN` | - | **transient** | `sleep` |
| 2568365 | `S,SN` | - | **transient** | `sleep` |
| 2568367 | `S,SN` | - | **transient** | `sleep` |
| 2568372 | `S,SN` | - | **transient** | `sleep` |
| 2568377 | `S,SN` | - | **transient** | `sleep` |
| 2568379 | `S,SN` | - | **transient** | `sleep` |
| 2568381 | `S,SN` | - | **transient** | `sleep` |
| 2568389 | `S,SN` | - | **transient** | `sleep` |
| 2568392 | `S,SN` | - | **transient** | `sleep` |
| 2568394 | `S,SN` | - | **transient** | `sleep` |
| 2568396 | `S,SN` | - | **transient** | `sleep` |
| 2568413 | `S,SN` | - | **transient** | `sleep` |
| 2568414 | `S,SN` | - | **transient** | `sleep` |
| 2568416 | `S,SN` | - | **transient** | `sleep` |
| 2568418 | `S,SN` | - | **transient** | `sleep` |
| 2568422 | `S,SN` | - | **transient** | `sleep` |
| 2570575 | `S,SN` | - | **transient** | `sleep` |
| 2570597 | `S` | - | **transient** | `systemd-userwor` |
| 2570603 | `S` | - | **transient** | `systemd-userwor` |
| 2570604 | `S` | - | **transient** | `systemd-userwor` |
| 2570630 | `S,SN` | - | **transient** | `sleep` |
| 2570634 | `S,SN` | - | **transient** | `sleep` |
| 2570637 | `S,SN` | - | **transient** | `sleep` |
| 2570642 | `S,SN` | - | **transient** | `sleep` |
| 2570647 | `S,SN` | - | **transient** | `sleep` |
| 2570649 | `S,SN` | - | **transient** | `sleep` |
| 2570651 | `S,SN` | - | **transient** | `sleep` |
| 2570654 | `S,SN` | - | **transient** | `sleep` |
| 2570656 | `S,SN` | - | **transient** | `sleep` |
| 2570658 | `S,SN` | - | **transient** | `sleep` |
| 2570668 | `S,SN` | - | **transient** | `sleep` |
| 2570676 | `S,SN` | - | **transient** | `sleep` |
| 2570678 | `S,SN` | - | **transient** | `sleep` |
| 2570680 | `S,SN` | - | **transient** | `sleep` |
| 2570684 | `S,SN` | - | **transient** | `sleep` |
| 2571526 | `S,SN` | - | **transient** | `sleep` |
| 2571538 | `S,SN` | - | **transient** | `sleep` |
| 2571543 | `S,SN` | - | **transient** | `sleep` |
| 2571545 | `S,SN` | - | **transient** | `sleep` |
| 2571548 | `S,SN` | - | **transient** | `sleep` |
| 2571551 | `S,SN` | - | **transient** | `sleep` |
| 2571553 | `S,SN` | - | **transient** | `sleep` |
| 2571555 | `S,SN` | - | **transient** | `sleep` |
| 2571572 | `S,SN` | - | **transient** | `sleep` |
| 2571573 | `S,SN` | - | **transient** | `sleep` |
| 2571575 | `S,SN` | - | **transient** | `sleep` |
| 2571577 | `S,SN` | - | **transient** | `sleep` |
| 2571581 | `S,SN` | - | **transient** | `sleep` |
| 2571584 | `S,SN` | - | **transient** | `sleep` |
| 2571586 | `S,SN` | - | **transient** | `sleep` |
| 2571588 | `S,SN` | - | **transient** | `sleep` |
| 2572441 | `S,SN` | - | **transient** | `sleep` |
| 2572460 | `S,SN` | - | **transient** | `sleep` |
| 2572462 | `S,SN` | - | **transient** | `sleep` |
| 2572464 | `S,SN` | - | **transient** | `sleep` |
| 2572472 | `S,SN` | - | **transient** | `sleep` |
| 2572474 | `S,SN` | - | **transient** | `sleep` |
| 2572477 | `S,SN` | - | **transient** | `sleep` |
| 2572479 | `S,SN` | - | **transient** | `sleep` |
| 2572490 | `S,SN` | - | **transient** | `sleep` |
| 2572494 | `S,SN` | - | **transient** | `sleep` |
| 2572496 | `S,SN` | - | **transient** | `sleep` |
| 2572498 | `S,SN` | - | **transient** | `sleep` |
| 2572500 | `S,SN` | - | **transient** | `sleep` |
| 2572501 | `S,SN` | - | **transient** | `sleep` |
| 2572503 | `S,SN` | - | **transient** | `sleep` |
| 2572505 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.87 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.54 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.60 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.45 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.12 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.38 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.02 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.60 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.60 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.84 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.15 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.21 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ssn-sink-passA.log` / `leg2-ssn-sink-passA`  (5 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.02 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.02 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.00 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.04 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.03 | both ends | `irqbalance` |
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
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.06 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
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
| 2423 | `S,Ssl` | 0.01 | both ends | `kunifiedpush-di` |
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
| 4094 | `S,Ss` | 0.06 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 9.83 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.01 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.05 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.15 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.05 | both ends | `org_kde_powerde` |
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
| 59466 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59660 | `S,Ssl` | 0.00 | both ends | `kitty` |
| 59662 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.42 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 3.21 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.12 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.12 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.02 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2555079 | `S,SNs` | 0.02 | both ends | `bash` |
| 2558299 | `S,SNs` | 0.00 | both ends | `bash` |
| 2570597 | `S` | - | **transient** | `systemd-userwor` |
| 2570603 | `S` | - | **transient** | `systemd-userwor` |
| 2570604 | `S` | - | **transient** | `systemd-userwor` |
| 2572441 | `S,SN` | - | **transient** | `sleep` |
| 2572460 | `S,SN` | - | **transient** | `sleep` |
| 2572462 | `S,SN` | - | **transient** | `sleep` |
| 2572464 | `S,SN` | - | **transient** | `sleep` |
| 2572474 | `S,SN` | - | **transient** | `sleep` |
| 2572477 | `S,SN` | - | **transient** | `sleep` |
| 2572479 | `S,SN` | - | **transient** | `sleep` |
| 2572490 | `S,SN` | - | **transient** | `sleep` |
| 2572494 | `S,SN` | - | **transient** | `sleep` |
| 2572496 | `S,SN` | - | **transient** | `sleep` |
| 2572498 | `S,SN` | - | **transient** | `sleep` |
| 2572500 | `S,SN` | - | **transient** | `sleep` |
| 2572501 | `S,SN` | - | **transient** | `sleep` |
| 2572503 | `S,SN` | - | **transient** | `sleep` |
| 2572505 | `S,SN` | - | **transient** | `sleep` |
| 2573572 | `S,SN` | - | **transient** | `sleep` |
| 2574504 | `S` | - | **transient** | `systemd-userwor` |
| 2574508 | `S` | - | **transient** | `systemd-userwor` |
| 2574509 | `S` | - | **transient** | `systemd-userwor` |
| 2574668 | `S,SN` | - | **transient** | `sleep` |
| 2574689 | `SN` | - | **transient** | `sleep` |
| 2574701 | `S,SN` | - | **transient** | `sleep` |
| 2574718 | `S,SN` | - | **transient** | `sleep` |
| 2574720 | `S,SN` | - | **transient** | `sleep` |
| 2574723 | `S,SN` | - | **transient** | `sleep` |
| 2574727 | `S,SN` | - | **transient** | `sleep` |
| 2574729 | `S,SN` | - | **transient** | `sleep` |
| 2574731 | `S,SN` | - | **transient** | `sleep` |
| 2574733 | `S,SN` | - | **transient** | `sleep` |
| 2574734 | `S,SN` | - | **transient** | `sleep` |
| 2574736 | `S,SN` | - | **transient** | `sleep` |
| 2574738 | `S,SN` | - | **transient** | `sleep` |
| 2574740 | `S,SN` | - | **transient** | `sleep` |
| 2574743 | `S,SN` | - | **transient** | `sleep` |
| 2574745 | `S,SN` | - | **transient** | `sleep` |
| 2575565 | `S,SN` | - | **transient** | `sleep` |
| 2575598 | `S,SN` | - | **transient** | `sleep` |
| 2575615 | `S,SN` | - | **transient** | `sleep` |
| 2575617 | `S,SN` | - | **transient** | `sleep` |
| 2575620 | `S,SN` | - | **transient** | `sleep` |
| 2575623 | `S,SN` | - | **transient** | `sleep` |
| 2575625 | `S,SN` | - | **transient** | `sleep` |
| 2575628 | `S,SN` | - | **transient** | `sleep` |
| 2575630 | `S,SN` | - | **transient** | `sleep` |
| 2575639 | `S,SN` | - | **transient** | `sleep` |
| 2575641 | `S,SN` | - | **transient** | `sleep` |
| 2575643 | `S,SN` | - | **transient** | `sleep` |
| 2575645 | `S,SN` | - | **transient** | `sleep` |
| 2575648 | `S,SN` | - | **transient** | `sleep` |
| 2575651 | `S,SN` | - | **transient** | `sleep` |
| 2575654 | `S,SN` | - | **transient** | `sleep` |
| 2576437 | `S` | - | **transient** | `systemd-userwor` |
| 2576441 | `S` | - | **transient** | `systemd-userwor` |
| 2576442 | `S` | - | **transient** | `systemd-userwor` |
| 2576481 | `S,SN` | - | **transient** | `sleep` |
| 2576522 | `S,SN` | - | **transient** | `sleep` |
| 2576532 | `S,SN` | - | **transient** | `sleep` |
| 2576534 | `S,SN` | - | **transient** | `sleep` |
| 2576540 | `S,SN` | - | **transient** | `sleep` |
| 2576542 | `S,SN` | - | **transient** | `sleep` |
| 2576544 | `S,SN` | - | **transient** | `sleep` |
| 2576547 | `S,SN` | - | **transient** | `sleep` |
| 2576548 | `S,SN` | - | **transient** | `sleep` |
| 2576550 | `S,SN` | - | **transient** | `sleep` |
| 2576552 | `S,SN` | - | **transient** | `sleep` |
| 2576554 | `S,SN` | - | **transient** | `sleep` |
| 2576557 | `S,SN` | - | **transient** | `sleep` |
| 2576560 | `S,SN` | - | **transient** | `sleep` |
| 2576563 | `S,SN` | - | **transient** | `sleep` |
| 2576565 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.76 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.46 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.58 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.44 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.19 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.41 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.02 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.57 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.58 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.83 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.11 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.11 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-ssn-sink-passB.log` / `leg2-ssn-sink-passB`  (5 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.00 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
| 697 | `S,Ss` | 0.00 | both ends | `systemd-udevd` |
| 919 | `S,S<sl` | 0.00 | both ends | `auditd` |
| 921 | `S,S<` | 0.00 | both ends | `sedispatch` |
| 951 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 963 | `S` | 0.02 | both ends | `dbus-broker` |
| 964 | `S,S<Ls` | 0.00 | both ends | `earlyoom` |
| 968 | `S,Ss` | 0.02 | both ends | `avahi-daemon` |
| 969 | `S,Ss` | 0.00 | both ends | `bluetoothd` |
| 975 | `S,Ssl` | 0.00 | both ends | `firewalld` |
| 977 | `S,Ssl` | 0.03 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.02 | both ends | `irqbalance` |
| 980 | `S,Ss` | 0.00 | both ends | `chronyd` |
| 991 | `S,Ssl` | 0.00 | both ends | `polkitd` |
| 993 | `S,SNsl` | 0.00 | both ends | `rtkit-daemon` |
| 995 | `S,Ss` | 0.00 | both ends | `smartd` |
| 997 | `S,Ssl` | 0.00 | both ends | `switcheroo-cont` |
| 999 | `S,Ssl` | 0.00 | both ends | `udisksd` |
| 1000 | `S,Ssl` | 0.00 | both ends | `upowerd` |
| 1025 | `S` | 0.00 | both ends | `avahi-daemon` |
| 1030 | `S,Ssl` | 0.01 | both ends | `accounts-daemon` |
| 1040 | `S,Ss` | 0.02 | both ends | `systemd-logind` |
| 1041 | `S,SNs` | 0.00 | both ends | `alsactl` |
| 1068 | `S,Ssl` | 0.00 | both ends | `abrtd` |
| 1112 | `S,Ssl` | 0.00 | both ends | `ModemManager` |
| 1152 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1154 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1155 | `S,Ss` | 0.00 | both ends | `abrt-dump-journ` |
| 1218 | `S,Ss` | 0.02 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.01 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.19 | both ends | `tailscaled` |
| 1246 | `S,Ssl` | 0.02 | both ends | `tuned` |
| 1309 | `S,Ssl` | 0.01 | both ends | `tuned-ppd` |
| 1375 | `S,Ssl` | 0.03 | both ends | `rsyslogd` |
| 1389 | `S,Ss` | 0.00 | both ends | `atd` |
| 1392 | `S,Ss` | 0.00 | both ends | `crond` |
| 1409 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1616 | `S` | 0.00 | both ends | `(sd-pam)` |
| 1635 | `S,Ss` | 0.00 | both ends | `dbus-broker-lau` |
| 1636 | `S` | 0.00 | both ends | `dbus-broker` |
| 1953 | `S,Ssl` | 0.00 | both ends | `uresourced` |
| 1962 | `S,SNsl` | 0.00 | both ends | `baloo_file` |
| 1965 | `S,S<sl` | 0.02 | both ends | `pipewire` |
| 1967 | `S,S<sl` | 0.03 | both ends | `wireplumber` |
| 2066 | `S,Ssl` | 0.00 | both ends | `at-spi-bus-laun` |
| 2080 | `S` | 0.00 | both ends | `dbus-broker-lau` |
| 2082 | `S` | 0.00 | both ends | `dbus-broker` |
| 2104 | `S,Ssl` | 0.00 | both ends | `at-spi2-registr` |
| 2162 | `S,Ssl` | 0.00 | both ends | `dconf-service` |
| 2187 | `S,Ss` | 0.00 | both ends | `ssh-agent` |
| 2236 | `S,S<Lsl` | 0.02 | both ends | `pipewire-pulse` |
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
| 4094 | `S,Ss` | 0.08 | both ends | `tmux:` |
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
| 50122 | `S,Sl` | 9.86 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.03 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.17 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.02 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.01 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.08 | both ends | `org_kde_powerde` |
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
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.43 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 3.35 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.13 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.13 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2555079 | `S,SNs` | - | **transient** | `bash` |
| 2558299 | `S,SNs` | 0.02 | both ends | `bash` |
| 2576437 | `S` | - | **transient** | `systemd-userwor` |
| 2576441 | `S` | - | **transient** | `systemd-userwor` |
| 2576442 | `S` | - | **transient** | `systemd-userwor` |
| 2576481 | `S,SN` | - | **transient** | `sleep` |
| 2576522 | `S,SN` | - | **transient** | `sleep` |
| 2576532 | `S,SN` | - | **transient** | `sleep` |
| 2576534 | `S,SN` | - | **transient** | `sleep` |
| 2576540 | `S,SN` | - | **transient** | `sleep` |
| 2576542 | `S,SN` | - | **transient** | `sleep` |
| 2576544 | `S,SN` | - | **transient** | `sleep` |
| 2576547 | `S,SN` | - | **transient** | `sleep` |
| 2576548 | `S,SN` | - | **transient** | `sleep` |
| 2576550 | `S,SN` | - | **transient** | `sleep` |
| 2576552 | `S,SN` | - | **transient** | `sleep` |
| 2576554 | `S,SN` | - | **transient** | `sleep` |
| 2576557 | `S,SN` | - | **transient** | `sleep` |
| 2576560 | `S,SN` | - | **transient** | `sleep` |
| 2576563 | `S,SN` | - | **transient** | `sleep` |
| 2576565 | `S,SN` | - | **transient** | `sleep` |
| 2578718 | `S,SN` | - | **transient** | `sleep` |
| 2578771 | `S,SN` | - | **transient** | `sleep` |
| 2578773 | `S,SN` | - | **transient** | `sleep` |
| 2578778 | `S,SN` | - | **transient** | `sleep` |
| 2578781 | `S,SN` | - | **transient** | `sleep` |
| 2578783 | `S,SN` | - | **transient** | `sleep` |
| 2578785 | `S,SN` | - | **transient** | `sleep` |
| 2578789 | `S,SN` | - | **transient** | `sleep` |
| 2578791 | `S,SN` | - | **transient** | `sleep` |
| 2578794 | `S,SN` | - | **transient** | `sleep` |
| 2578797 | `S,SN` | - | **transient** | `sleep` |
| 2578800 | `S,SN` | - | **transient** | `sleep` |
| 2578802 | `S,SN` | - | **transient** | `sleep` |
| 2578805 | `S,SN` | - | **transient** | `sleep` |
| 2579595 | `S,SN` | - | **transient** | `sleep` |
| 2579642 | `S,Ssl` | - | **transient** | `tailscaled` |
| 2579649 | `S` | - | **transient** | `fish` |
| 2579778 | `S,S+` | - | **transient** | `tmux:` |
| 2579829 | `S,SN` | - | **transient** | `sleep` |
| 2579831 | `S,SN` | - | **transient** | `sleep` |
| 2579834 | `S,SN` | - | **transient** | `sleep` |
| 2579836 | `S,SN` | - | **transient** | `sleep` |
| 2579854 | `S,SN` | - | **transient** | `sleep` |
| 2579856 | `S,SN` | - | **transient** | `sleep` |
| 2579859 | `S,SN` | - | **transient** | `sleep` |
| 2579862 | `S,SN` | - | **transient** | `sleep` |
| 2579865 | `S,SN` | - | **transient** | `sleep` |
| 2579869 | `S,SN` | - | **transient** | `sleep` |
| 2579870 | `S` | - | **transient** | `systemd-userwor` |
| 2579871 | `S` | - | **transient** | `systemd-userwor` |
| 2579873 | `S,SN` | - | **transient** | `sleep` |
| 2579874 | `S,SN` | - | **transient** | `sleep` |
| 2579875 | `S` | - | **transient** | `systemd-userwor` |
| 2579891 | `S,SN` | - | **transient** | `sleep` |
| 2579893 | `S,SN` | - | **transient** | `sleep` |
| 2580770 | `S,SN` | - | **transient** | `sleep` |
| 2580772 | `S,SN` | - | **transient** | `sleep` |
| 2580774 | `S,SN` | - | **transient** | `sleep` |
| 2580782 | `S,SN` | - | **transient** | `sleep` |
| 2580784 | `S,SN` | - | **transient** | `sleep` |
| 2580787 | `S,SN` | - | **transient** | `sleep` |
| 2580790 | `S,SN` | - | **transient** | `sleep` |
| 2580793 | `S,SN` | - | **transient** | `sleep` |
| 2580796 | `S,SN` | - | **transient** | `sleep` |
| 2580798 | `S,SN` | - | **transient** | `sleep` |
| 2580799 | `S,SN` | - | **transient** | `sleep` |
| 2580815 | `S,SN` | - | **transient** | `sleep` |
| 2580817 | `S,SN` | - | **transient** | `sleep` |
| 2580819 | `S,SN` | - | **transient** | `sleep` |
| 2580820 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 1.82 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.52 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.59 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.46 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.22 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.18 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 1.39 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.59 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.60 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.85 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.12 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.15 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-walk-off-passA.log` / `leg2-walk-off-passA`  (5 snapshots)

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
| 964 | `S,S<Ls` | 0.01 | both ends | `earlyoom` |
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
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.03 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 3.68 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.03 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.04 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.00 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.03 | both ends | `org_kde_powerde` |
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
| 116643 | `S,Sl` | 0.15 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 1.13 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.08 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.09 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.01 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513318 | `S,SNs` | 0.00 | both ends | `bash` |
| 2517107 | `S` | - | **transient** | `systemd-userwor` |
| 2517109 | `S` | - | **transient** | `systemd-userwor` |
| 2517131 | `S` | - | **transient** | `systemd-userwor` |
| 2524190 | `S,SN` | - | **transient** | `sleep` |
| 2527155 | `S,SN` | - | **transient** | `sleep` |
| 2527190 | `S,SN` | - | **transient** | `sleep` |
| 2527874 | `S,SN` | - | **transient** | `sleep` |
| 2528594 | `S,SN` | - | **transient** | `sleep` |
| 2528629 | `S,SN` | - | **transient** | `sleep` |
| 2529918 | `S,SN` | - | **transient** | `sleep` |
| 2530633 | `S,SN` | - | **transient** | `sleep` |
| 2531350 | `S,SN` | - | **transient** | `sleep` |
| 2531384 | `S,SN` | - | **transient** | `sleep` |
| 2532080 | `S,SN` | - | **transient** | `sleep` |
| 2532484 | `S,SN` | - | **transient** | `sleep` |
| 2532500 | `S,SN` | - | **transient** | `sleep` |
| 2532817 | `S,SN` | - | **transient** | `sleep` |
| 2534206 | `S,SN` | - | **transient** | `sleep` |
| 2536288 | `S,SN` | - | **transient** | `sleep` |
| 2536293 | `S,SN` | - | **transient** | `sleep` |
| 2536296 | `S,SN` | - | **transient** | `sleep` |
| 2536298 | `S,SN` | - | **transient** | `sleep` |
| 2536331 | `S,SN` | - | **transient** | `sleep` |
| 2536333 | `S,SN` | - | **transient** | `sleep` |
| 2536335 | `S,SN` | - | **transient** | `sleep` |
| 2536336 | `S,SN` | - | **transient** | `sleep` |
| 2536338 | `S,SN` | - | **transient** | `sleep` |
| 2536340 | `S,SN` | - | **transient** | `sleep` |
| 2536342 | `S,SN` | - | **transient** | `sleep` |
| 2536343 | `S,SN` | - | **transient** | `sleep` |
| 2536345 | `S,SN` | - | **transient** | `sleep` |
| 2536348 | `S,SN` | - | **transient** | `sleep` |
| 2536350 | `S,SN` | - | **transient** | `sleep` |
| 2537060 | `S,SN` | - | **transient** | `sleep` |
| 2537080 | `S,SN` | - | **transient** | `sleep` |
| 2537082 | `S,SN` | - | **transient** | `sleep` |
| 2537084 | `S,SN` | - | **transient** | `sleep` |
| 2537086 | `S,SN` | - | **transient** | `sleep` |
| 2537088 | `S,SN` | - | **transient** | `sleep` |
| 2537091 | `S,SN` | - | **transient** | `sleep` |
| 2537092 | `S,SN` | - | **transient** | `sleep` |
| 2537094 | `S,SN` | - | **transient** | `sleep` |
| 2537097 | `S,SN` | - | **transient** | `sleep` |
| 2537099 | `S,SN` | - | **transient** | `sleep` |
| 2537102 | `S,SN` | - | **transient** | `sleep` |
| 2537120 | `S,SN` | - | **transient** | `sleep` |
| 2537122 | `S,SN` | - | **transient** | `sleep` |
| 2537794 | `S,SN` | - | **transient** | `sleep` |
| 2537807 | `S,SN` | - | **transient** | `sleep` |
| 2537810 | `S,SN` | - | **transient** | `sleep` |
| 2537817 | `S` | - | **transient** | `systemd-userwor` |
| 2537830 | `S` | - | **transient** | `systemd-userwor` |
| 2537854 | `S,SN` | - | **transient** | `sleep` |
| 2537856 | `S,SN` | - | **transient** | `sleep` |
| 2537857 | `S` | - | **transient** | `systemd-userwor` |
| 2537862 | `S,SN` | - | **transient** | `sleep` |
| 2537864 | `S,SN` | - | **transient** | `sleep` |
| 2537867 | `S,SN` | - | **transient** | `sleep` |
| 2537869 | `S,SN` | - | **transient** | `sleep` |
| 2537879 | `S,SN` | - | **transient** | `sleep` |
| 2537881 | `S,SN` | - | **transient** | `sleep` |
| 2537883 | `S,SN` | - | **transient** | `sleep` |
| 2537886 | `S,SN` | - | **transient** | `sleep` |
| 2537888 | `S,SN` | - | **transient** | `sleep` |
| 2537890 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.95 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.28 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.18 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.06 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.51 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.26 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.04 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-walk-off-passB.log` / `leg2-walk-off-passB`  (5 snapshots)

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
| 50122 | `S,Sl` | 4.00 | both ends | `kwin_wayland` |
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
| 50719 | `S,Ssl` | 0.03 | both ends | `org_kde_powerde` |
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
| 583906 | `S,SNs` | 0.01 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 1.19 | both ends | `claude` |
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
| 2453144 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513318 | `S,SNs` | - | **transient** | `bash` |
| 2537794 | `S,SN` | - | **transient** | `sleep` |
| 2537810 | `S,SN` | - | **transient** | `sleep` |
| 2537817 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2537830 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2537854 | `S,SN` | - | **transient** | `sleep` |
| 2537856 | `S,SN` | - | **transient** | `sleep` |
| 2537857 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2537862 | `S,SN` | - | **transient** | `sleep` |
| 2537864 | `S,SN` | - | **transient** | `sleep` |
| 2537867 | `S,SN` | - | **transient** | `sleep` |
| 2537869 | `S,SN` | - | **transient** | `sleep` |
| 2537879 | `S,SN` | - | **transient** | `sleep` |
| 2537881 | `S,SN` | - | **transient** | `sleep` |
| 2537883 | `S,SN` | - | **transient** | `sleep` |
| 2537886 | `S,SN` | - | **transient** | `sleep` |
| 2537888 | `S,SN` | - | **transient** | `sleep` |
| 2537890 | `S,SN` | - | **transient** | `sleep` |
| 2539051 | `S,SN` | - | **transient** | `sleep` |
| 2539905 | `S,SN` | - | **transient** | `sleep` |
| 2539907 | `S,SN` | - | **transient** | `sleep` |
| 2539909 | `S,SN` | - | **transient** | `sleep` |
| 2539926 | `S,SN` | - | **transient** | `sleep` |
| 2539929 | `S,SN` | - | **transient** | `sleep` |
| 2539932 | `S,SN` | - | **transient** | `sleep` |
| 2539937 | `S,SN` | - | **transient** | `sleep` |
| 2539940 | `S,SN` | - | **transient** | `sleep` |
| 2539956 | `S,SN` | - | **transient** | `sleep` |
| 2539958 | `S,SN` | - | **transient** | `sleep` |
| 2539959 | `S,SN` | - | **transient** | `sleep` |
| 2539963 | `S,SN` | - | **transient** | `sleep` |
| 2539968 | `S,SN` | - | **transient** | `sleep` |
| 2540636 | `S,SN` | - | **transient** | `sleep` |
| 2540668 | `S,SN` | - | **transient** | `sleep` |
| 2540682 | `S,SN` | - | **transient** | `sleep` |
| 2540687 | `S,SN` | - | **transient** | `sleep` |
| 2540689 | `S,SN` | - | **transient** | `sleep` |
| 2540690 | `S,SN` | - | **transient** | `sleep` |
| 2540692 | `S,SN` | - | **transient** | `sleep` |
| 2540694 | `S,SN` | - | **transient** | `sleep` |
| 2540696 | `S,SN` | - | **transient** | `sleep` |
| 2540699 | `S,SN` | - | **transient** | `sleep` |
| 2540700 | `S,SN` | - | **transient** | `sleep` |
| 2540702 | `S,SN` | - | **transient** | `sleep` |
| 2540704 | `S,SN` | - | **transient** | `sleep` |
| 2540707 | `S,SN` | - | **transient** | `sleep` |
| 2541388 | `S,SN` | - | **transient** | `sleep` |
| 2541392 | `S,SN` | - | **transient** | `sleep` |
| 2541394 | `S,SN` | - | **transient** | `sleep` |
| 2541411 | `S,SN` | - | **transient** | `sleep` |
| 2541414 | `S,SN` | - | **transient** | `sleep` |
| 2541416 | `S,SN` | - | **transient** | `sleep` |
| 2541418 | `S,SN` | - | **transient** | `sleep` |
| 2541436 | `S,SN` | - | **transient** | `sleep` |
| 2541438 | `S,SN` | - | **transient** | `sleep` |
| 2541439 | `S,SN` | - | **transient** | `sleep` |
| 2541441 | `S,SN` | - | **transient** | `sleep` |
| 2541443 | `S,SN` | - | **transient** | `sleep` |
| 2541445 | `S,SN` | - | **transient** | `sleep` |
| 2541448 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.64 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.23 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.15 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.06 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.54 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.22 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.28 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.05 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.08 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-walk-sink-passA.log` / `leg2-walk-sink-passA`  (5 snapshots)

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
| 1967 | `S,S<sl` | 0.02 | both ends | `wireplumber` |
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
| 50122 | `S,Sl` | 3.81 | both ends | `kwin_wayland` |
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
| 59662 | `S,Sl` | 0.01 | both ends | `kitten` |
| 59665 | `S,Ss+` | 0.00 | both ends | `fish` |
| 59671 | `S,Sl` | 0.01 | both ends | `kitten` |
| 68064 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 78046 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 82588 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 113543 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 116643 | `S,Sl` | 0.15 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 1.18 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.08 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.08 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2537817 | `S` | - | **transient** | `systemd-userwor` |
| 2537830 | `S` | - | **transient** | `systemd-userwor` |
| 2537857 | `S` | - | **transient** | `systemd-userwor` |
| 2541394 | `S,SN` | - | **transient** | `sleep` |
| 2541414 | `S,SN` | - | **transient** | `sleep` |
| 2541416 | `S,SN` | - | **transient** | `sleep` |
| 2541436 | `S,SN` | - | **transient** | `sleep` |
| 2541438 | `S,SN` | - | **transient** | `sleep` |
| 2541441 | `S,SN` | - | **transient** | `sleep` |
| 2541443 | `S,SN` | - | **transient** | `sleep` |
| 2541445 | `S,SN` | - | **transient** | `sleep` |
| 2541448 | `S,SN` | - | **transient** | `sleep` |
| 2542744 | `S,SN` | - | **transient** | `sleep` |
| 2542746 | `S,SN` | - | **transient** | `sleep` |
| 2542748 | `S,SN` | - | **transient** | `sleep` |
| 2542751 | `S,SN` | - | **transient** | `sleep` |
| 2542752 | `S,SN` | - | **transient** | `sleep` |
| 2543460 | `S,SN` | - | **transient** | `sleep` |
| 2543483 | `S,SN` | - | **transient** | `sleep` |
| 2543489 | `S,SN` | - | **transient** | `sleep` |
| 2543492 | `S,SN` | - | **transient** | `sleep` |
| 2543494 | `S,SN` | - | **transient** | `sleep` |
| 2543497 | `S,SN` | - | **transient** | `sleep` |
| 2543500 | `S,SN` | - | **transient** | `sleep` |
| 2543503 | `S,SN` | - | **transient** | `sleep` |
| 2543523 | `S,SN` | - | **transient** | `sleep` |
| 2543525 | `S,SN` | - | **transient** | `sleep` |
| 2543526 | `S,SN` | - | **transient** | `sleep` |
| 2543528 | `S,SN` | - | **transient** | `sleep` |
| 2543538 | `S,SN` | - | **transient** | `sleep` |
| 2544196 | `S,SN` | - | **transient** | `sleep` |
| 2544245 | `S,SN` | - | **transient** | `sleep` |
| 2544247 | `S,SN` | - | **transient** | `sleep` |
| 2544251 | `S,SN` | - | **transient** | `sleep` |
| 2544254 | `S,SN` | - | **transient** | `sleep` |
| 2544257 | `S,SN` | - | **transient** | `sleep` |
| 2544264 | `S,SN` | - | **transient** | `sleep` |
| 2544266 | `S,SN` | - | **transient** | `sleep` |
| 2544268 | `S,SN` | - | **transient** | `sleep` |
| 2544271 | `S,SN` | - | **transient** | `sleep` |
| 2544273 | `S,SN` | - | **transient** | `sleep` |
| 2544274 | `S,SN` | - | **transient** | `sleep` |
| 2544276 | `S,SN` | - | **transient** | `sleep` |
| 2544278 | `S,SN` | - | **transient** | `sleep` |
| 2544965 | `S,SN` | - | **transient** | `sleep` |
| 2544967 | `S,SN` | - | **transient** | `sleep` |
| 2544969 | `S,SN` | - | **transient** | `sleep` |
| 2544985 | `S,SN` | - | **transient** | `sleep` |
| 2544989 | `S,SN` | - | **transient** | `sleep` |
| 2544991 | `S,SN` | - | **transient** | `sleep` |
| 2544993 | `S,SN` | - | **transient** | `sleep` |
| 2544996 | `S` | - | **transient** | `systemd-userwor` |
| 2544998 | `S,SN` | - | **transient** | `sleep` |
| 2544999 | `S` | - | **transient** | `systemd-userwor` |
| 2545016 | `S,SN` | - | **transient** | `sleep` |
| 2545019 | `S,SN` | - | **transient** | `sleep` |
| 2545020 | `S,SN` | - | **transient** | `sleep` |
| 2545021 | `S` | - | **transient** | `systemd-userwor` |
| 2545023 | `S,SN` | - | **transient** | `sleep` |
| 2545025 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.62 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.12 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.23 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.16 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.53 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.27 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.04 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.03 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L6-leg2-walk-sink-passB.log` / `leg2-walk-sink-passB`  (5 snapshots)

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
| 50122 | `Rl,S,Sl` | 3.78 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.15 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 1.13 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.08 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.06 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2544268 | `S,SN` | - | **transient** | `sleep` |
| 2544965 | `S,SN` | - | **transient** | `sleep` |
| 2544967 | `S,SN` | - | **transient** | `sleep` |
| 2544969 | `S,SN` | - | **transient** | `sleep` |
| 2544985 | `S,SN` | - | **transient** | `sleep` |
| 2544989 | `S,SN` | - | **transient** | `sleep` |
| 2544991 | `S,SN` | - | **transient** | `sleep` |
| 2544993 | `S,SN` | - | **transient** | `sleep` |
| 2544996 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2544998 | `S,SN` | - | **transient** | `sleep` |
| 2544999 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2545016 | `S,SN` | - | **transient** | `sleep` |
| 2545019 | `S,SN` | - | **transient** | `sleep` |
| 2545020 | `S,SN` | - | **transient** | `sleep` |
| 2545021 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2545023 | `S,SN` | - | **transient** | `sleep` |
| 2545025 | `S,SN` | - | **transient** | `sleep` |
| 2546995 | `S,SN` | - | **transient** | `sleep` |
| 2547027 | `S,SN` | - | **transient** | `sleep` |
| 2547030 | `S,SN` | - | **transient** | `sleep` |
| 2547035 | `S,SN` | - | **transient** | `sleep` |
| 2547050 | `S,SN` | - | **transient** | `sleep` |
| 2547052 | `S,SN` | - | **transient** | `sleep` |
| 2547054 | `S,SN` | - | **transient** | `sleep` |
| 2547056 | `S,SN` | - | **transient** | `sleep` |
| 2547058 | `S,SN` | - | **transient** | `sleep` |
| 2547059 | `S,SN` | - | **transient** | `sleep` |
| 2547062 | `S,SN` | - | **transient** | `sleep` |
| 2547064 | `S,SN` | - | **transient** | `sleep` |
| 2547066 | `S,SN` | - | **transient** | `sleep` |
| 2547070 | `S,SN` | - | **transient** | `sleep` |
| 2547775 | `S,SN` | - | **transient** | `sleep` |
| 2547782 | `S,SN` | - | **transient** | `sleep` |
| 2547785 | `S,SN` | - | **transient** | `sleep` |
| 2547799 | `S,SN` | - | **transient** | `sleep` |
| 2547801 | `S,SN` | - | **transient** | `sleep` |
| 2547819 | `S,SN` | - | **transient** | `sleep` |
| 2547822 | `S,SN` | - | **transient** | `sleep` |
| 2547823 | `S,SN` | - | **transient** | `sleep` |
| 2547825 | `S,SN` | - | **transient** | `sleep` |
| 2547827 | `S,SN` | - | **transient** | `sleep` |
| 2547829 | `S,SN` | - | **transient** | `sleep` |
| 2547830 | `S,SN` | - | **transient** | `sleep` |
| 2547832 | `S,SN` | - | **transient** | `sleep` |
| 2547834 | `S,SN` | - | **transient** | `sleep` |
| 2548525 | `S,SN` | - | **transient** | `sleep` |
| 2548528 | `S,SN` | - | **transient** | `sleep` |
| 2548545 | `S,SN` | - | **transient** | `sleep` |
| 2548547 | `S,SN` | - | **transient** | `sleep` |
| 2548549 | `S,SN` | - | **transient** | `sleep` |
| 2548551 | `S,SN` | - | **transient** | `sleep` |
| 2548556 | `S,SN` | - | **transient** | `sleep` |
| 2548557 | `S,SN` | - | **transient** | `sleep` |
| 2548558 | `S,SN` | - | **transient** | `sleep` |
| 2548562 | `S,SN` | - | **transient** | `sleep` |
| 2548564 | `S,SN` | - | **transient** | `sleep` |
| 2548567 | `S,SN` | - | **transient** | `sleep` |
| 2548588 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.85 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.24 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.21 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.08 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.54 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.28 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L7-interior-perfA.log` / `interior-perfA`  (5 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.00 | both ends | `systemd-journal` |
| 682 | `S,Ss` | 0.01 | both ends | `systemd-userdbd` |
| 694 | `S,Ss` | 0.01 | both ends | `systemd-resolve` |
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
| 1244 | `S,Ssl` | 0.03 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 3.25 | both ends | `kwin_wayland` |
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
| 116643 | `S,Sl` | 0.16 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 1.20 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.08 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.07 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.02 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2558299 | `S,SNs` | 0.00 | both ends | `bash` |
| 2579870 | `S` | - | **transient** | `systemd-userwor` |
| 2579871 | `S` | - | **transient** | `systemd-userwor` |
| 2579875 | `S` | - | **transient** | `systemd-userwor` |
| 2583637 | `S,SN` | - | **transient** | `sleep` |
| 2584358 | `SN` | - | **transient** | `sleep` |
| 2584375 | `S,SN` | - | **transient** | `sleep` |
| 2584377 | `S,SN` | - | **transient** | `sleep` |
| 2584395 | `S,SN` | - | **transient** | `sleep` |
| 2584397 | `S,SN` | - | **transient** | `sleep` |
| 2584399 | `S,SN` | - | **transient** | `sleep` |
| 2584401 | `S,SN` | - | **transient** | `sleep` |
| 2584402 | `SN` | - | **transient** | `sleep` |
| 2584418 | `S,SN` | - | **transient** | `sleep` |
| 2584422 | `S,SN` | - | **transient** | `sleep` |
| 2584424 | `S,SN` | - | **transient** | `sleep` |
| 2584427 | `S,SN` | - | **transient** | `sleep` |
| 2584430 | `S,SN` | - | **transient** | `sleep` |
| 2584433 | `S,SN` | - | **transient** | `sleep` |
| 2586451 | `S,SN` | - | **transient** | `sleep` |
| 2586457 | `S,SN` | - | **transient** | `sleep` |
| 2586475 | `S,SN` | - | **transient** | `sleep` |
| 2586480 | `S,SN` | - | **transient** | `sleep` |
| 2586483 | `S,SN` | - | **transient** | `sleep` |
| 2586486 | `S,SN` | - | **transient** | `sleep` |
| 2586488 | `S,SN` | - | **transient** | `sleep` |
| 2586509 | `S,SN` | - | **transient** | `sleep` |
| 2586520 | `S,SN` | - | **transient** | `sleep` |
| 2586522 | `S,SN` | - | **transient** | `sleep` |
| 2586523 | `S,SN` | - | **transient** | `sleep` |
| 2586539 | `S,SN` | - | **transient** | `sleep` |
| 2586541 | `S,SN` | - | **transient** | `sleep` |
| 2586544 | `S,SN` | - | **transient** | `sleep` |
| 2587227 | `S,SN` | - | **transient** | `sleep` |
| 2587236 | `S,SN` | - | **transient** | `sleep` |
| 2587240 | `S,SN` | - | **transient** | `sleep` |
| 2587261 | `S,SN` | - | **transient** | `sleep` |
| 2587264 | `S,SN` | - | **transient** | `sleep` |
| 2587267 | `S,SN` | - | **transient** | `sleep` |
| 2587268 | `S` | - | **transient** | `systemd-userwor` |
| 2587269 | `S,SN` | - | **transient** | `sleep` |
| 2587271 | `S,SN` | - | **transient** | `sleep` |
| 2587279 | `S` | - | **transient** | `systemd-userwor` |
| 2587287 | `S` | - | **transient** | `systemd-userwor` |
| 2587289 | `S,SN` | - | **transient** | `sleep` |
| 2587291 | `S,SN` | - | **transient** | `sleep` |
| 2587309 | `S,SN` | - | **transient** | `sleep` |
| 2587310 | `S,SN` | - | **transient** | `sleep` |
| 2587312 | `S,SN` | - | **transient** | `sleep` |
| 2587314 | `S,SN` | - | **transient** | `sleep` |
| 2587316 | `S,SN` | - | **transient** | `sleep` |
| 2588010 | `SN` | - | **transient** | `sleep` |
| 2588038 | `S,SN` | - | **transient** | `sleep` |
| 2588054 | `S,SN` | - | **transient** | `sleep` |
| 2588056 | `S,SN` | - | **transient** | `sleep` |
| 2588060 | `S,SN` | - | **transient** | `sleep` |
| 2588078 | `S,SN` | - | **transient** | `sleep` |
| 2588080 | `S,SN` | - | **transient** | `sleep` |
| 2588082 | `S,SN` | - | **transient** | `sleep` |
| 2588084 | `S,SN` | - | **transient** | `sleep` |
| 2588085 | `S,SN` | - | **transient** | `sleep` |
| 2588095 | `S,SN` | - | **transient** | `sleep` |
| 2588099 | `S,SN` | - | **transient** | `sleep` |
| 2588101 | `S,SN` | - | **transient** | `sleep` |
| 2588105 | `S,SN` | - | **transient** | `sleep` |
| 2588363 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.63 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.22 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.17 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.06 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.48 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.32 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.04 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L7-interior-perfB.log` / `interior-perfB`  (5 snapshots)

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
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.00 | both ends | `gssproxy` |
| 1243 | `S,Ss` | 0.00 | both ends | `sshd` |
| 1244 | `S,Ssl` | 0.03 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 3.01 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.00 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.06 | both ends | `plasmashell` |
| 50651 | `S,Ssl` | 0.00 | both ends | `xdg-desktop-por` |
| 50658 | `S,Sl` | 0.01 | both ends | `mysqld` |
| 50682 | `S,Ssl` | 0.00 | both ends | `kactivitymanage` |
| 50711 | `S,Ssl` | 0.00 | both ends | `gmenudbusmenupr` |
| 50712 | `S,Ssl` | 0.00 | both ends | `kaccess` |
| 50718 | `S,Ssl` | 0.00 | both ends | `polkit-kde-auth` |
| 50719 | `S,Ssl` | 0.03 | both ends | `org_kde_powerde` |
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
| 116643 | `S,Sl` | 0.17 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 1.21 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.08 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.10 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.01 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.01 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.01 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2622320 | `S,SNs` | 0.00 | both ends | `bash` |
| 2623894 | `S,SN` | - | **transient** | `sleep` |
| 2623932 | `SN` | - | **transient** | `sleep` |
| 2623941 | `S,SN` | - | **transient** | `sleep` |
| 2623943 | `S,SN` | - | **transient** | `sleep` |
| 2623952 | `S,SN` | - | **transient** | `sleep` |
| 2623963 | `S,SN` | - | **transient** | `sleep` |
| 2623966 | `S,SN` | - | **transient** | `sleep` |
| 2623968 | `S,SN` | - | **transient** | `sleep` |
| 2623971 | `S,SN` | - | **transient** | `sleep` |
| 2623975 | `S,SN` | - | **transient** | `sleep` |
| 2623976 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2623979 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2623980 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2623982 | `S,SN` | - | **transient** | `sleep` |
| 2625290 | `S,SN` | - | **transient** | `sleep` |
| 2625292 | `S,SN` | - | **transient** | `sleep` |
| 2625293 | `S,SN` | - | **transient** | `sleep` |
| 2625295 | `S,SN` | - | **transient** | `sleep` |
| 2626030 | `S,SN` | - | **transient** | `sleep` |
| 2626032 | `S,SN` | - | **transient** | `sleep` |
| 2626034 | `S,SN` | - | **transient** | `sleep` |
| 2626043 | `S,SN` | - | **transient** | `sleep` |
| 2626045 | `S,SN` | - | **transient** | `sleep` |
| 2626047 | `S,SN` | - | **transient** | `sleep` |
| 2626065 | `S,SN` | - | **transient** | `sleep` |
| 2626067 | `S,SN` | - | **transient** | `sleep` |
| 2626068 | `S,SN` | - | **transient** | `sleep` |
| 2626070 | `S,SN` | - | **transient** | `sleep` |
| 2626072 | `S,SN` | - | **transient** | `sleep` |
| 2626081 | `S,SN` | - | **transient** | `sleep` |
| 2626083 | `S,SN` | - | **transient** | `sleep` |
| 2626092 | `S,SN` | - | **transient** | `sleep` |
| 2626786 | `S,SN` | - | **transient** | `sleep` |
| 2626810 | `S,SN` | - | **transient** | `sleep` |
| 2626812 | `S,SN` | - | **transient** | `sleep` |
| 2626814 | `S,SN` | - | **transient** | `sleep` |
| 2626817 | `S,SN` | - | **transient** | `sleep` |
| 2626819 | `S,SN` | - | **transient** | `sleep` |
| 2626848 | `S,SN` | - | **transient** | `sleep` |
| 2626850 | `S,SN` | - | **transient** | `sleep` |
| 2626852 | `S,SN` | - | **transient** | `sleep` |
| 2626861 | `S,SN` | - | **transient** | `sleep` |
| 2626865 | `S,SN` | - | **transient** | `sleep` |
| 2626866 | `S,SN` | - | **transient** | `sleep` |
| 2626868 | `S,SN` | - | **transient** | `sleep` |
| 2626887 | `S,SN` | - | **transient** | `sleep` |
| 2627555 | `S,SN` | - | **transient** | `sleep` |
| 2627561 | `S,SN` | - | **transient** | `sleep` |
| 2627563 | `S,SN` | - | **transient** | `sleep` |
| 2627584 | `S,SN` | - | **transient** | `sleep` |
| 2627593 | `S,SN` | - | **transient** | `sleep` |
| 2627595 | `S,SN` | - | **transient** | `sleep` |
| 2627604 | `S,SN` | - | **transient** | `sleep` |
| 2627607 | `S,SN` | - | **transient** | `sleep` |
| 2627626 | `S,SN` | - | **transient** | `sleep` |
| 2627627 | `S,SN` | - | **transient** | `sleep` |
| 2627629 | `S,SN` | - | **transient** | `sleep` |
| 2627631 | `S,SN` | - | **transient** | `sleep` |
| 2627633 | `S,SN` | - | **transient** | `sleep` |
| 2627637 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.57 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.48 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.17 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.06 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.49 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.31 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.04 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L7-interior-wall.log` / `interior-wall`  (5 snapshots)

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
| 1244 | `S,Ssl` | 0.02 | both ends | `tailscaled` |
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
| 50122 | `S,Sl` | 3.11 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.04 | both ends | `kded6` |
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
| 50988 | `S,Sl` | 0.01 | both ends | `akonadi_migrati` |
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
| 116643 | `S,Sl` | 0.16 | both ends | `kscreenlocker_g` |
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
| 1097257 | `S,Sl+` | 1.21 | both ends | `claude` |
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
| 2453174 | `S,SNs` | 0.02 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.01 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2558299 | `S,SNs` | 0.00 | both ends | `bash` |
| 2579870 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2579871 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2579875 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2580770 | `S,SN` | - | **transient** | `sleep` |
| 2580772 | `S,SN` | - | **transient** | `sleep` |
| 2580774 | `S,SN` | - | **transient** | `sleep` |
| 2580782 | `S,SN` | - | **transient** | `sleep` |
| 2580787 | `S,SN` | - | **transient** | `sleep` |
| 2580790 | `S,SN` | - | **transient** | `sleep` |
| 2580793 | `S,SN` | - | **transient** | `sleep` |
| 2580796 | `S,SN` | - | **transient** | `sleep` |
| 2580798 | `S,SN` | - | **transient** | `sleep` |
| 2580799 | `S,SN` | - | **transient** | `sleep` |
| 2580815 | `S,SN` | - | **transient** | `sleep` |
| 2580817 | `S,SN` | - | **transient** | `sleep` |
| 2580819 | `S,SN` | - | **transient** | `sleep` |
| 2580820 | `S,SN` | - | **transient** | `sleep` |
| 2582151 | `S,SN` | - | **transient** | `sleep` |
| 2582810 | `S,SN` | - | **transient** | `sleep` |
| 2582813 | `S,SN` | - | **transient** | `sleep` |
| 2582816 | `S,SN` | - | **transient** | `sleep` |
| 2582831 | `S,SN` | - | **transient** | `sleep` |
| 2582840 | `S,SN` | - | **transient** | `sleep` |
| 2582842 | `S,SN` | - | **transient** | `sleep` |
| 2582843 | `S,SN` | - | **transient** | `sleep` |
| 2582859 | `S,SN` | - | **transient** | `sleep` |
| 2582861 | `S,SN` | - | **transient** | `sleep` |
| 2582878 | `S,SN` | - | **transient** | `sleep` |
| 2582880 | `S,SN` | - | **transient** | `sleep` |
| 2582882 | `S,SN` | - | **transient** | `sleep` |
| 2582884 | `S,SN` | - | **transient** | `sleep` |
| 2582902 | `S,SN` | - | **transient** | `sleep` |
| 2583607 | `S,SN` | - | **transient** | `sleep` |
| 2583625 | `S,SN` | - | **transient** | `sleep` |
| 2583626 | `S,SN` | - | **transient** | `sleep` |
| 2583636 | `S,SN` | - | **transient** | `sleep` |
| 2583637 | `S,SN` | - | **transient** | `sleep` |
| 2583639 | `S,SN` | - | **transient** | `sleep` |
| 2583654 | `S,SN` | - | **transient** | `sleep` |
| 2583656 | `S,SN` | - | **transient** | `sleep` |
| 2583657 | `S,SN` | - | **transient** | `sleep` |
| 2583659 | `S,SN` | - | **transient** | `sleep` |
| 2583661 | `S,SN` | - | **transient** | `sleep` |
| 2583664 | `S,SN` | - | **transient** | `sleep` |
| 2583667 | `S,SN` | - | **transient** | `sleep` |
| 2583669 | `S,SN` | - | **transient** | `sleep` |
| 2584358 | `S,SN` | - | **transient** | `sleep` |
| 2584375 | `S,SN` | - | **transient** | `sleep` |
| 2584377 | `S,SN` | - | **transient** | `sleep` |
| 2584395 | `S,SN` | - | **transient** | `sleep` |
| 2584397 | `S,SN` | - | **transient** | `sleep` |
| 2584399 | `S,SN` | - | **transient** | `sleep` |
| 2584401 | `S,SN` | - | **transient** | `sleep` |
| 2584402 | `S,SN` | - | **transient** | `sleep` |
| 2584418 | `S,SN` | - | **transient** | `sleep` |
| 2584422 | `S,SN` | - | **transient** | `sleep` |
| 2584424 | `S,SN` | - | **transient** | `sleep` |
| 2584427 | `S,SN` | - | **transient** | `sleep` |
| 2584430 | `S,SN` | - | **transient** | `sleep` |
| 2584433 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.61 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.13 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.19 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.07 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.51 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.18 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.18 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.30 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.04 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.05 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L8-interior-cells-r1.log` / `interior-cells-r1`  (13 snapshots)

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
| 50122 | `S,Sl` | 1.77 | both ends | `kwin_wayland` |
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
| 59662 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 1097257 | `S,Sl+` | 0.60 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.18 | both ends | `codex` |
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
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2653834 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2654515 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2654516 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2654518 | `S,SN` | - | **transient** | `sleep` |
| 2655296 | `S,SN` | - | **transient** | `sleep` |
| 2655298 | `S,SN` | - | **transient** | `sleep` |
| 2655485 | `S,SN` | - | **transient** | `sleep` |
| 2656626 | `S,SN` | - | **transient** | `sleep` |
| 2656628 | `S,SN` | - | **transient** | `sleep` |
| 2656630 | `S,SN` | - | **transient** | `sleep` |
| 2656645 | `S,SN` | - | **transient** | `sleep` |
| 2656647 | `S,SN` | - | **transient** | `sleep` |
| 2656648 | `S,SN` | - | **transient** | `sleep` |
| 2656650 | `S,SN` | - | **transient** | `sleep` |
| 2656652 | `S,SN` | - | **transient** | `sleep` |
| 2656671 | `S,SN` | - | **transient** | `sleep` |
| 2656673 | `S,SN` | - | **transient** | `sleep` |
| 2657363 | `S,SN` | - | **transient** | `sleep` |
| 2658066 | `S,SN` | - | **transient** | `sleep` |
| 2658082 | `S,SN` | - | **transient** | `sleep` |
| 2658205 | `S,SN` | - | **transient** | `sleep` |
| 2658298 | `S,SN` | - | **transient** | `sleep` |
| 2658770 | `S,SN` | - | **transient** | `sleep` |
| 2660164 | `S,SN` | - | **transient** | `sleep` |
| 2660194 | `S,SN` | - | **transient** | `sleep` |
| 2660870 | `S,SN` | - | **transient** | `sleep` |
| 2660886 | `S,SN` | - | **transient** | `sleep` |
| 2660901 | `S,SN` | - | **transient** | `sleep` |
| 2661577 | `S,SN` | - | **transient** | `sleep` |
| 2661595 | `S,SN` | - | **transient** | `sleep` |
| 2662295 | `S,SN` | - | **transient** | `sleep` |
| 2662297 | `S,SN` | - | **transient** | `sleep` |
| 2662299 | `S,SN` | - | **transient** | `sleep` |
| 2663004 | `S,SN` | - | **transient** | `sleep` |
| 2663020 | `S,SN` | - | **transient** | `sleep` |
| 2663036 | `S,SN` | - | **transient** | `sleep` |
| 2663038 | `S,SN` | - | **transient** | `sleep` |
| 2663040 | `S,SN` | - | **transient** | `sleep` |
| 2663717 | `S,SN` | - | **transient** | `sleep` |
| 2663719 | `S,SN` | - | **transient** | `sleep` |
| 2663736 | `S,SN` | - | **transient** | `sleep` |
| 2663752 | `S,SN` | - | **transient** | `sleep` |
| 2663754 | `S,SN` | - | **transient** | `sleep` |
| 2663755 | `S,SN` | - | **transient** | `sleep` |
| 2664431 | `S,SN` | - | **transient** | `sleep` |
| 2664433 | `S,SN` | - | **transient** | `sleep` |
| 2664449 | `S,SN` | - | **transient** | `sleep` |
| 2664458 | `S,SN` | - | **transient** | `sleep` |
| 2664788 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.28 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.08 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.26 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.10 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.16 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.04 | both ends | `2.1.263` |
| 4022478 | `RNl,S,SNl` | 0.06 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L8-interior-cells-r2.log` / `interior-cells-r2`  (13 snapshots)

| pid | states seen | delta (s) | presence | command |
|---|---|---|---|---|
| 655 | `S,Ss` | 0.01 | both ends | `systemd-journal` |
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
| 1218 | `S,Ss` | 0.01 | both ends | `wpa_supplicant` |
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
| 50122 | `S,Sl` | 1.85 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.00 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.02 | both ends | `kded6` |
| 50628 | `S,Sl` | 0.00 | both ends | `akonadiserver` |
| 50634 | `S,Ssl` | 0.04 | both ends | `plasmashell` |
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
| 58924 | `S,Ssl` | 0.01 | both ends | `fwupd` |
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
| 583887 | `S,SNs` | 0.01 | both ends | `bash` |
| 583906 | `S,SNs` | 0.00 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `R,S,Sl+` | 0.68 | both ends | `claude` |
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
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.01 | both ends | `bash` |
| 2653834 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2654515 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2654516 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2660194 | `S,SN` | - | **transient** | `sleep` |
| 2663036 | `S,SN` | - | **transient** | `sleep` |
| 2663038 | `S,SN` | - | **transient** | `sleep` |
| 2663040 | `S,SN` | - | **transient** | `sleep` |
| 2663717 | `S,SN` | - | **transient** | `sleep` |
| 2663736 | `S,SN` | - | **transient** | `sleep` |
| 2663752 | `S,SN` | - | **transient** | `sleep` |
| 2663754 | `S,SN` | - | **transient** | `sleep` |
| 2663755 | `S,SN` | - | **transient** | `sleep` |
| 2664431 | `S,SN` | - | **transient** | `sleep` |
| 2664433 | `S,SN` | - | **transient** | `sleep` |
| 2664449 | `S,SN` | - | **transient** | `sleep` |
| 2664458 | `S,SN` | - | **transient** | `sleep` |
| 2664788 | `S,SN` | - | **transient** | `sleep` |
| 2666474 | `S,SN` | - | **transient** | `sleep` |
| 2666491 | `S,SN` | - | **transient** | `sleep` |
| 2666860 | `S,SN` | - | **transient** | `sleep` |
| 2666880 | `S,SN` | - | **transient** | `sleep` |
| 2667179 | `S,SN` | - | **transient** | `sleep` |
| 2668588 | `S,SN` | - | **transient** | `sleep` |
| 2668884 | `S,SN` | - | **transient** | `sleep` |
| 2669305 | `S,SN` | - | **transient** | `sleep` |
| 2669578 | `S,SN` | - | **transient** | `sleep` |
| 2669979 | `S,SN` | - | **transient** | `sleep` |
| 2669995 | `S,SN` | - | **transient** | `sleep` |
| 2670012 | `S,SN` | - | **transient** | `sleep` |
| 2670692 | `S,SN` | - | **transient** | `sleep` |
| 2670708 | `S,SN` | - | **transient** | `sleep` |
| 2670731 | `S,SN` | - | **transient** | `sleep` |
| 2671443 | `S,SN` | - | **transient** | `sleep` |
| 2671691 | `S,SN` | - | **transient** | `sleep` |
| 2672119 | `S,SN` | - | **transient** | `sleep` |
| 2672120 | `S,SN` | - | **transient** | `sleep` |
| 2672136 | `S,SN` | - | **transient** | `sleep` |
| 2672151 | `S,SN` | - | **transient** | `sleep` |
| 2672821 | `S,SN` | - | **transient** | `sleep` |
| 2672823 | `S,SN` | - | **transient** | `sleep` |
| 2672835 | `S,SN` | - | **transient** | `sleep` |
| 2672837 | `S,SN` | - | **transient** | `sleep` |
| 2672838 | `S,SN` | - | **transient** | `sleep` |
| 2672854 | `S,SN` | - | **transient** | `sleep` |
| 2672856 | `S,SN` | - | **transient** | `sleep` |
| 2672872 | `S,SN` | - | **transient** | `sleep` |
| 2672881 | `S,SN` | - | **transient** | `sleep` |
| 2673572 | `S,SN` | - | **transient** | `sleep` |
| 2673588 | `S,SN` | - | **transient** | `sleep` |
| 2673590 | `S,SN` | - | **transient** | `sleep` |
| 2673592 | `S,SN` | - | **transient** | `sleep` |
| 2673594 | `S,SN` | - | **transient** | `sleep` |
| 2673611 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.36 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.16 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.12 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.27 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.17 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.03 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `L8-interior-cells-r3.log` / `interior-cells-r3`  (13 snapshots)

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
| 50122 | `R,S,Sl` | 1.95 | both ends | `kwin_wayland` |
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
| 1097257 | `Rl+,S,Sl+` | 0.67 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.18 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.18 | both ends | `codex` |
| 1862197 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 2446564 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448394 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448483 | `S,SNs` | 0.00 | both ends | `bash` |
| 2448492 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453144 | `S,SNs` | 0.00 | both ends | `bash` |
| 2453174 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455500 | `S,SNs` | 0.00 | both ends | `bash` |
| 2455510 | `S,SNs` | 0.00 | both ends | `bash` |
| 2470969 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2512521 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513212 | `S,SNs` | 0.00 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2622320 | `S,SNs` | 0.00 | both ends | `bash` |
| 2623976 | `S` | - | **transient** | `systemd-userwor` |
| 2623979 | `S` | - | **transient** | `systemd-userwor` |
| 2623980 | `S` | - | **transient** | `systemd-userwor` |
| 2641707 | `S,SN` | - | **transient** | `sleep` |
| 2644634 | `S,SN` | - | **transient** | `sleep` |
| 2644738 | `S,SN` | - | **transient** | `sleep` |
| 2645299 | `S,SN` | - | **transient** | `sleep` |
| 2645317 | `S,SN` | - | **transient** | `sleep` |
| 2645334 | `S,SN` | - | **transient** | `sleep` |
| 2645337 | `S,SN` | - | **transient** | `sleep` |
| 2645339 | `S,SN` | - | **transient** | `sleep` |
| 2645707 | `S,SN` | - | **transient** | `sleep` |
| 2646019 | `S,SN` | - | **transient** | `sleep` |
| 2646021 | `S,SN` | - | **transient** | `sleep` |
| 2646045 | `S,SN` | - | **transient** | `sleep` |
| 2646047 | `S,SN` | - | **transient** | `sleep` |
| 2646049 | `S,SN` | - | **transient** | `sleep` |
| 2646072 | `S,SN` | - | **transient** | `sleep` |
| 2648087 | `S,SN` | - | **transient** | `sleep` |
| 2648782 | `S,SN` | - | **transient** | `sleep` |
| 2648812 | `S,SN` | - | **transient** | `sleep` |
| 2648814 | `S,SN` | - | **transient** | `sleep` |
| 2649384 | `S,SN` | - | **transient** | `sleep` |
| 2649488 | `S,SN` | - | **transient** | `sleep` |
| 2649490 | `S,SN` | - | **transient** | `sleep` |
| 2649530 | `S,SN` | - | **transient** | `sleep` |
| 2650240 | `S,SN` | - | **transient** | `sleep` |
| 2650922 | `S,SN` | - | **transient** | `sleep` |
| 2650924 | `S,SN` | - | **transient** | `sleep` |
| 2651634 | `S,SN` | - | **transient** | `sleep` |
| 2651636 | `S,SN` | - | **transient** | `sleep` |
| 2651659 | `S,SN` | - | **transient** | `sleep` |
| 2651675 | `S,SN` | - | **transient** | `sleep` |
| 2652279 | `S,SN` | - | **transient** | `sleep` |
| 2653076 | `S,SN` | - | **transient** | `sleep` |
| 2653091 | `S,SN` | - | **transient** | `sleep` |
| 2653093 | `S,SN` | - | **transient** | `sleep` |
| 2653109 | `S,SN` | - | **transient** | `sleep` |
| 2653111 | `S,SN` | - | **transient** | `sleep` |
| 2653129 | `S,SN` | - | **transient** | `sleep` |
| 2653833 | `S,SN` | - | **transient** | `sleep` |
| 2653834 | `S` | - | **transient** | `systemd-userwor` |
| 2654515 | `S` | - | **transient** | `systemd-userwor` |
| 2654516 | `S` | - | **transient** | `systemd-userwor` |
| 2654518 | `S,SN` | - | **transient** | `sleep` |
| 2654520 | `S,SN` | - | **transient** | `sleep` |
| 2654522 | `S,SN` | - | **transient** | `sleep` |
| 2654537 | `S,SN` | - | **transient** | `sleep` |
| 2654541 | `S,SN` | - | **transient** | `sleep` |
| 2654543 | `S,SN` | - | **transient** | `sleep` |
| 2654569 | `S,SN` | - | **transient** | `sleep` |
| 2654570 | `S,SN` | - | **transient** | `sleep` |
| 2654572 | `S,SN` | - | **transient** | `sleep` |
| 2654581 | `S,SN` | - | **transient** | `sleep` |
| 2655278 | `S,SN` | - | **transient** | `sleep` |
| 2655294 | `S,SN` | - | **transient** | `sleep` |
| 2655296 | `S,SN` | - | **transient** | `sleep` |
| 2655298 | `S,SN` | - | **transient** | `sleep` |
| 2655485 | `S,SN` | - | **transient** | `sleep` |
| 2721602 | `S,Ssl` | 0.29 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.07 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.09 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.04 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.27 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.19 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |
