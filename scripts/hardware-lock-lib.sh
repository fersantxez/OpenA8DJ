#!/usr/bin/env bash

opena8dj_lock_root() {
  printf '%s\n' "${AUDIO_GATE_LOCK_ROOT:-$HOME/.opena8dj/hardware-gate.lock}"
}

opena8dj_pid_alive() {
  local pid="$1"
  [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

opena8dj_cleanup_stale_lock() {
  local lock_root="$1"
  local owner_pid=""
  if [ -f "$lock_root/owner" ]; then
    owner_pid="$(awk -F= '/^pid=/{print $2; exit}' "$lock_root/owner" || true)"
  fi
  if [ -n "$owner_pid" ] && ! opena8dj_pid_alive "$owner_pid"; then
    rm -rf "$lock_root"
  fi
}

opena8dj_acquire_hardware_lock() {
  local worktree="$1"
  local purpose="$2"
  local evidence="$3"
  local actions="$4"
  local forbidden="$5"
  local lock_root
  lock_root="$(opena8dj_lock_root)"

  opena8dj_cleanup_stale_lock "$lock_root"

  if [ "${OPENA8DJ_HARDWARE_LOCK_HELD:-0}" = "1" ]; then
    local owner_pid=""
    if [ -f "$lock_root/owner" ]; then
      owner_pid="$(awk -F= '/^pid=/{print $2; exit}' "$lock_root/owner" || true)"
    fi
    if [ -n "$owner_pid" ] && opena8dj_pid_alive "$owner_pid"; then
      _OPENA8DJ_LOCK_ACQUIRED=0
      export OPENA8DJ_HARDWARE_LOCK_HELD=1
      return 0
    fi
  fi

  if ! mkdir "$lock_root" 2>/dev/null; then
    echo "audio_gate_lock=BUSY"
    cat "$lock_root/owner" 2>/dev/null || true
    return 75
  fi

  _OPENA8DJ_LOCK_ACQUIRED=1
  export OPENA8DJ_HARDWARE_LOCK_HELD=1
  {
    echo "pid=$$"
    echo "owner=codex"
    echo "worktree=$worktree"
    echo "purpose=$purpose"
    echo "started_at=$(date +%Y-%m-%dT%H:%M:%S%z)"
    echo "actions_authorized=$actions"
    echo "forbidden=$forbidden"
    echo "evidence=$evidence"
  } > "$lock_root/owner"
}

opena8dj_release_hardware_lock() {
  if [ "${_OPENA8DJ_LOCK_ACQUIRED:-0}" = "1" ]; then
    rm -rf "$(opena8dj_lock_root)"
  fi
}
