#!/bin/bash
set -euo pipefail

export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/flatpak/bus}"

user_dir=$(realpath ~/.local/share/flatpak)
system_dir=/var/lib/flatpak

fail () {
  echo "bazaar-host-bwrap: $*" >&2
  exit 1
}

args=(--clearenv)
fds=()
while [ $# -gt 0 ]; do
  case "$1" in
    --)
      args+=("$@")
      break ;;
    --unshare-all|--share-net|--unshare-user|--unshare-user-try|--unshare-ipc|\
    --unshare-pid|--unshare-net|--unshare-uts|--unshare-cgroup|\
    --unshare-cgroup-try|--die-with-parent|--as-pid-1|--new-session|\
    --clearenv|--disable-userns|--assert-userns-disabled|--level-prefix)
      n=0 ;;
    --argv0|--uid|--gid|--hostname|--chdir|--unsetenv|--lock-file|\
    --remount-ro|--exec-label|--file-label|--proc|--dev|--tmpfs|--mqueue|\
    --dir|--cap-add|--cap-drop|--perms|--size|--overlay-src|--tmp-overlay|\
    --ro-overlay)
      n=1 ;;
    --setenv|--bind|--bind-try|--dev-bind|--dev-bind-try|--ro-bind|\
    --ro-bind-try|--symlink|--chmod)
      n=2 ;;
    --sync-fd|--seccomp|--add-seccomp-fd|--block-fd|--userns-block-fd|\
    --info-fd|--json-status-fd)
      n=1 fds+=("--forward-fd=$2") ;;
    --file|--bind-data|--ro-bind-data)
      n=2 fds+=("--forward-fd=$2") ;;
    --bind-fd|--ro-bind-fd)
      path=$(readlink "/proc/self/fd/$2")
      case "$1:$path" in
        --ro-bind-fd:"$user_dir"/*|--ro-bind-fd:"$system_dir"/*|--bind-fd:"$user_dir"/*) ;;
        *) fail "refusing $1 for $path" ;;
      esac
      set -- "${1%-fd}" "$path" "${@:3}"
      n=2 ;;
    *)
      fail "unsupported option $1" ;;
  esac
  args+=("${@:1:n+1}")
  shift $((n + 1))
done

exec flatpak-spawn --host --clear-env "${fds[@]}" \
  --env=PATH=/usr/local/bin:/usr/bin:/bin \
  sh -c 'b=/usr/libexec/flatpak-bwrap; [ -x "$b" ] || b=bwrap; exec "$b" "$@"' \
  bazaar-host-bwrap "${args[@]}"
