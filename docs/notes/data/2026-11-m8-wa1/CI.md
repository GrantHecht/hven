# WA-1 CI record

Run 44459486250 (`gh workflow run ci.yml --ref wa-1`, head `ee9847ad9`: the code commit `0d674bdcb` + the DECLARATION commit, which
touches no code path): **completed success** on all four lanes -- linux-clang-release, linux-clang-coverage, macos-clang-release,
windows-clang-release (no STL1000 at configure: `origin/m8` 66cc44ccc carries the Windows fix). The evidence commits after it touch
`docs/` only.
