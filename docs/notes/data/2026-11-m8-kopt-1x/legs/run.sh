#!/usr/bin/env bash
# generic wrapper: flock + setsid -w + pipefail. usage: run.sh <script> [args]
exec flock /tmp/box-build.lock setsid -w bash "$@"
