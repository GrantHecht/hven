# Every foreign pid of every batch, with every state observed for it

Settler ruling R13 (W5 T8.9r fix round 3): R2' asks for every foreign pid and state seen in any snapshot, listed -- not a count and a busiest-five. This is that list, written by `scripts/idle_proof.py` from the same snapshots the fractions are computed from.

States are the UNION of `/proc/<pid>/stat`'s single character (`FOREIGN_TICK`) and `ps`'s full string (`FOREIGN_PS`). A pid marked **transient** was present in some snapshot of the batch but not in both ends, so it has no CPU delta. `delta` is ticks/clk across the batch window for pids present at both ends.

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
| 1238 | `S,Ss` | 0.00 | both ends | `cupsd` |
| 1240 | `S,Ssl` | 0.01 | both ends | `gssproxy` |
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
| 1097257 | `S,Sl+` | 0.34 | both ends | `claude` |
| 1101947 | `S,Sl+` | 0.00 | both ends | `clangd.main` |
| 1312467 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1312476 | `S,Sl` | 0.07 | both ends | `codex` |
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
| 2722044 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2918873 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2918883 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2918884 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2919699 | `S,SN` | - | **transient** | `sleep` |
| 2919737 | `S,SN` | - | **transient** | `sleep` |
| 2919740 | `S,SN` | - | **transient** | `sleep` |
| 2919743 | `S,SN` | - | **transient** | `sleep` |
| 2919745 | `S,SN` | - | **transient** | `sleep` |
| 2919747 | `S,SN` | - | **transient** | `sleep` |
| 2919767 | `S,SN` | - | **transient** | `sleep` |
| 2919774 | `S,SN` | - | **transient** | `sleep` |
| 2919776 | `S,SN` | - | **transient** | `sleep` |
| 2919778 | `S,SN` | - | **transient** | `sleep` |
| 2919780 | `S,SN` | - | **transient** | `sleep` |
| 2919782 | `S,SN` | - | **transient** | `sleep` |
| 2919783 | `S,SN` | - | **transient** | `sleep` |
| 2920461 | `S,SN` | - | **transient** | `sleep` |
| 2920472 | `S,SN` | - | **transient** | `sleep` |
| 2920474 | `S,SN` | - | **transient** | `sleep` |
| 2920476 | `S,SN` | - | **transient** | `sleep` |
| 2921138 | `S,SN` | - | **transient** | `sleep` |
| 2921141 | `S,SN` | - | **transient** | `sleep` |
| 2921157 | `S,SN` | - | **transient** | `sleep` |
| 2921159 | `S,SN` | - | **transient** | `sleep` |
| 2921160 | `S,SN` | - | **transient** | `sleep` |
| 2921448 | `S,SN` | - | **transient** | `sleep` |
| 2921823 | `S,SN` | - | **transient** | `sleep` |
| 2921825 | `S,SN` | - | **transient** | `sleep` |
| 2922486 | `S,SN` | - | **transient** | `sleep` |
| 2922488 | `S,SN` | - | **transient** | `sleep` |
| 2922490 | `S,SN` | - | **transient** | `sleep` |
| 2922504 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.09 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.00 | both ends | `2.1.263` |
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
| 50122 | `R,S,Sl` | 1.00 | both ends | `kwin_wayland` |
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
| 59466 | `S,Sl` | 0.01 | both ends | `kitten` |
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
| 2513212 | `S,SNs` | 0.01 | both ends | `bash` |
| 2513256 | `S,SNs` | 0.00 | both ends | `bash` |
| 2721602 | `S,Ssl` | 0.03 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.04 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.17 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2918873 | `S` | - | **transient** | `systemd-userwor` |
| 2918883 | `S` | - | **transient** | `systemd-userwor` |
| 2918884 | `S` | - | **transient** | `systemd-userwor` |
| 2920472 | `S,SN` | - | **transient** | `sleep` |
| 2920476 | `S,SN` | - | **transient** | `sleep` |
| 2921138 | `S,SN` | - | **transient** | `sleep` |
| 2921141 | `S,SN` | - | **transient** | `sleep` |
| 2921157 | `S,SN` | - | **transient** | `sleep` |
| 2921159 | `S,SN` | - | **transient** | `sleep` |
| 2921448 | `S,SN` | - | **transient** | `sleep` |
| 2921823 | `S,SN` | - | **transient** | `sleep` |
| 2921825 | `S,SN` | - | **transient** | `sleep` |
| 2922486 | `S,SN` | - | **transient** | `sleep` |
| 2922488 | `S,SN` | 0.00 | both ends | `sleep` |
| 2922490 | `S,SN` | - | **transient** | `sleep` |
| 2922504 | `S,SN` | - | **transient** | `sleep` |
| 2924479 | `S,SN` | - | **transient** | `sleep` |
| 2924481 | `S,SN` | - | **transient** | `sleep` |
| 2924484 | `S,SN` | - | **transient** | `sleep` |
| 2924498 | `S,SN` | - | **transient** | `sleep` |
| 2924500 | `S,SN` | - | **transient** | `sleep` |
| 2924502 | `S,SN` | - | **transient** | `sleep` |
| 2925178 | `S,SN` | - | **transient** | `sleep` |
| 2925179 | `S,SN` | - | **transient** | `sleep` |
| 2925181 | `S,SN` | - | **transient** | `sleep` |
| 2925183 | `S,SN` | - | **transient** | `sleep` |
| 2925844 | `S,SN` | - | **transient** | `sleep` |
| 2925846 | `S,SN` | - | **transient** | `sleep` |
| 2925848 | `S,SN` | - | **transient** | `sleep` |
| 2925849 | `S` | - | **transient** | `systemd-userwor` |
| 2926509 | `S` | - | **transient** | `systemd-userwor` |
| 2926510 | `S` | - | **transient** | `systemd-userwor` |
| 2926511 | `S,SN` | - | **transient** | `sleep` |
| 2926515 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.07 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.02 | both ends | `2.1.263` |
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
| 50122 | `R,S,Sl` | 0.92 | both ends | `kwin_wayland` |
| 50125 | `S,Ssl` | 0.01 | both ends | `kalendarac` |
| 50350 | `S,Ssl` | 0.00 | both ends | `imsettings-daem` |
| 50356 | `S,Ssl` | 0.00 | both ends | `drkonqi-coredum` |
| 50463 | `S,Sl` | 0.00 | both ends | `plasma-keyboard` |
| 50471 | `S` | 0.00 | both ends | `Xwayland` |
| 50511 | `S,Ssl` | 0.00 | both ends | `akonadi_control` |
| 50568 | `S,Ssl` | 0.00 | both ends | `ksmserver` |
| 50573 | `S,Ssl` | 0.04 | both ends | `kded6` |
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
| 583906 | `S,SNs` | 0.01 | both ends | `bash` |
| 667180 | `S,SLsl` | 0.00 | both ends | `kwalletd6` |
| 1097257 | `S,Sl+` | 0.32 | both ends | `claude` |
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
| 2721730 | `S,Sl` | 0.06 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.20 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.03 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.13 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.01 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2922488 | `S,SN` | - | **transient** | `sleep` |
| 2924484 | `S,SN` | - | **transient** | `sleep` |
| 2924498 | `S,SN` | - | **transient** | `sleep` |
| 2924502 | `S,SN` | - | **transient** | `sleep` |
| 2925178 | `S,SN` | - | **transient** | `sleep` |
| 2925181 | `SN` | - | **transient** | `sleep` |
| 2925183 | `S,SN` | - | **transient** | `sleep` |
| 2925844 | `S,SN` | - | **transient** | `sleep` |
| 2925846 | `S,SN` | - | **transient** | `sleep` |
| 2925848 | `S,SN` | - | **transient** | `sleep` |
| 2925849 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926509 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926510 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926511 | `S,SN` | - | **transient** | `sleep` |
| 2926515 | `S,SN` | - | **transient** | `sleep` |
| 2927611 | `S,SN` | - | **transient** | `sleep` |
| 2928314 | `S,SN` | - | **transient** | `sleep` |
| 2928495 | `S,SN` | - | **transient** | `sleep` |
| 2928498 | `S,SN` | - | **transient** | `sleep` |
| 2929160 | `S,SN` | - | **transient** | `sleep` |
| 2929176 | `S,SN` | - | **transient** | `sleep` |
| 2929178 | `S,SN` | - | **transient** | `sleep` |
| 2929179 | `S,SN` | - | **transient** | `sleep` |
| 2929181 | `S,SN` | - | **transient** | `sleep` |
| 2929844 | `S,SN` | - | **transient** | `sleep` |
| 2929846 | `S,SN` | - | **transient** | `sleep` |
| 2929848 | `S,SN` | - | **transient** | `sleep` |
| 2929850 | `S,SN` | - | **transient** | `sleep` |
| 2929853 | `S,SN` | - | **transient** | `sleep` |
| 2930517 | `S,SN` | - | **transient** | `sleep` |
| 2931143 | `S,SN` | - | **transient** | `sleep` |
| 2931172 | `S,SN` | - | **transient** | `sleep` |
| 2931175 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.05 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.09 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
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
| 977 | `S,Ssl` | 0.01 | both ends | `NetworkManager` |
| 979 | `S,Ssl` | 0.00 | both ends | `irqbalance` |
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
| 1244 | `S,Ssl` | 0.00 | both ends | `tailscaled` |
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
| 1097257 | `S,Sl+` | 0.34 | both ends | `claude` |
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
| 2722044 | `S,Sl` | 0.16 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.02 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.01 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.00 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.04 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2925849 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926509 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926510 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2928498 | `SN` | - | **transient** | `sleep` |
| 2929160 | `S,SN` | - | **transient** | `sleep` |
| 2929178 | `S,SN` | - | **transient** | `sleep` |
| 2929181 | `S,SN` | - | **transient** | `sleep` |
| 2929844 | `S,SN` | - | **transient** | `sleep` |
| 2929846 | `S,SN` | - | **transient** | `sleep` |
| 2929848 | `S,SN` | - | **transient** | `sleep` |
| 2929850 | `S,SN` | 0.00 | both ends | `sleep` |
| 2929853 | `S,SN` | - | **transient** | `sleep` |
| 2930517 | `S,SN` | - | **transient** | `sleep` |
| 2931143 | `S,SN` | - | **transient** | `sleep` |
| 2931172 | `S,SN` | - | **transient** | `sleep` |
| 2931175 | `S,SN` | - | **transient** | `sleep` |
| 2932351 | `S,SN` | - | **transient** | `sleep` |
| 2932508 | `S,SN` | - | **transient** | `sleep` |
| 2932510 | `S,SN` | - | **transient** | `sleep` |
| 2932526 | `S,SN` | - | **transient** | `sleep` |
| 2932527 | `S,SN` | - | **transient** | `sleep` |
| 2932529 | `S,SN` | - | **transient** | `sleep` |
| 2932946 | `S,SN` | - | **transient** | `sleep` |
| 2933198 | `S,SN` | - | **transient** | `sleep` |
| 2933859 | `S,SN` | - | **transient** | `sleep` |
| 2933861 | `S,SN` | - | **transient** | `sleep` |
| 2933863 | `S,SN` | - | **transient** | `sleep` |
| 2934524 | `S,SN` | - | **transient** | `sleep` |
| 2934528 | `S,SN` | - | **transient** | `sleep` |
| 2934530 | `S,SN` | - | **transient** | `sleep` |
| 2934532 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.07 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.01 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |

## `X5-xwall-r5.log` / `xwall-r5`  (6 snapshots)

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
| 50122 | `R,Rl,S,Sl` | 0.88 | both ends | `kwin_wayland` |
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
| 1312476 | `S,Sl` | 0.07 | both ends | `codex` |
| 1312934 | `S,Sl` | 0.00 | both ends | `codex-code-mode` |
| 1417566 | `S,Sl` | 0.00 | both ends | `Web` |
| 1861772 | `S,Ssl` | 0.00 | both ends | `node-22` |
| 1861779 | `S,Sl` | 0.07 | both ends | `codex` |
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
| 2721602 | `S,Ssl` | 0.34 | both ends | `firefox` |
| 2721623 | `S,Sl` | 0.00 | both ends | `crashhelper` |
| 2721703 | `S` | 0.00 | both ends | `forkserver` |
| 2721721 | `S,Sl` | 0.00 | both ends | `Socket` |
| 2721730 | `S,Sl` | 0.15 | both ends | `WebExtensions` |
| 2721739 | `S,Sl` | 0.00 | both ends | `RDD` |
| 2722015 | `S,Ssl` | 0.00 | both ends | `pcscd` |
| 2722044 | `S,Sl` | 0.08 | both ends | `Isolated` |
| 2722083 | `S,Sl` | 0.00 | both ends | `Utility` |
| 2722106 | `S,Sl` | 0.06 | both ends | `Isolated` |
| 2722108 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 2722196 | `S,Sl` | 0.02 | both ends | `Privileged` |
| 2722303 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722314 | `S,Sl` | 0.11 | both ends | `Isolated` |
| 2722361 | `S` | 0.00 | both ends | `sd_espeak-ng` |
| 2722384 | `S,Sl` | 0.00 | both ends | `sd_dummy` |
| 2722387 | `S,Ssl` | 0.00 | both ends | `speech-dispatch` |
| 2722912 | `S,Sl` | 0.07 | both ends | `Isolated` |
| 2864209 | `S,SNs` | 0.00 | both ends | `launch-astra-t8` |
| 2864212 | `S,SN` | 0.00 | both ends | `sleep` |
| 2925849 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926509 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2926510 | `S` | 0.00 | both ends | `systemd-userwor` |
| 2929850 | `S,SN` | - | **transient** | `sleep` |
| 2932351 | `S,SN` | - | **transient** | `sleep` |
| 2932510 | `S,SN` | - | **transient** | `sleep` |
| 2932526 | `S,SN` | - | **transient** | `sleep` |
| 2932946 | `S,SN` | - | **transient** | `sleep` |
| 2933198 | `S,SN` | - | **transient** | `sleep` |
| 2933859 | `S,SN` | - | **transient** | `sleep` |
| 2933861 | `S,SN` | - | **transient** | `sleep` |
| 2933863 | `S,SN` | - | **transient** | `sleep` |
| 2934524 | `S,SN` | - | **transient** | `sleep` |
| 2934528 | `S,SN` | - | **transient** | `sleep` |
| 2934530 | `S,SN` | - | **transient** | `sleep` |
| 2934532 | `S,SN` | - | **transient** | `sleep` |
| 2936516 | `S,SN` | - | **transient** | `sleep` |
| 2936526 | `S,SN` | - | **transient** | `sleep` |
| 2936542 | `S,SN` | - | **transient** | `sleep` |
| 2936544 | `S,SN` | - | **transient** | `sleep` |
| 2936545 | `S,SN` | - | **transient** | `sleep` |
| 2937206 | `S,SN` | - | **transient** | `sleep` |
| 2937219 | `S,SN` | - | **transient** | `sleep` |
| 2937870 | `S,SN` | - | **transient** | `sleep` |
| 2937872 | `S,SN` | - | **transient** | `sleep` |
| 2937874 | `S,SN` | - | **transient** | `sleep` |
| 2937876 | `S,SN` | - | **transient** | `sleep` |
| 2938331 | `S,SN` | - | **transient** | `sleep` |
| 2938541 | `S,SN` | - | **transient** | `sleep` |
| 2938543 | `S,SN` | - | **transient** | `sleep` |
| 2938546 | `S,SN` | - | **transient** | `sleep` |
| 3691695 | `S,Sl` | 0.00 | both ends | `Web` |
| 3692061 | `S,Sl` | 0.00 | both ends | `Web` |
| 3819500 | `S,Sl` | 0.03 | both ends | `Isolated` |
| 4022442 | `S,Ssl` | 0.08 | both ends | `claude` |
| 4022457 | `S,SNsl` | 0.01 | both ends | `2.1.263` |
| 4022478 | `S,SNl` | 0.02 | both ends | `2.1.263` |
| 4162368 | `S,SNl+` | 0.00 | both ends | `clangd.main` |
