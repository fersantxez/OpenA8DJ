#!/usr/bin/env bash

opena8dj_lock_root() {
  printf '%s\n' "${AUDIO_GATE_LOCK_ROOT:-$HOME/.opena8dj/hardware-gate.lock}"
}

opena8dj_lock_supervisor() {
  local root="$1" script_path="$2" purpose="$3" evidence="$4" actions="$5" forbidden="$6"
  shift 6
  /usr/bin/python3 "$(dirname "$script_path")/hardware-lock-supervisor.py" \
    --lock-root "$root" \
    --worktree "$root" \
    --purpose "$purpose" \
    --evidence "$evidence" \
    --actions "$actions" \
    --forbidden "$forbidden" \
    -- "$script_path" "$@"
}

opena8dj_supervise_hardware_operation() {
  local worktree="$1" script_path="$2" purpose="$3" evidence="$4" actions="$5" forbidden="$6"
  shift 6
  local lock_root
  lock_root="$(opena8dj_lock_root)"
  if OPENA8DJ_LOCK_ROOT="$lock_root" /usr/bin/python3 "$(dirname "$script_path")/hardware-lock-supervisor.py" \
      --verify-inherited --lock-root "$lock_root" >/dev/null 2>&1; then
    export OPENA8DJ_HARDWARE_LOCK_HELD=1
    return 0
  fi
  exec /usr/bin/python3 "$(dirname "$script_path")/hardware-lock-supervisor.py" \
    --lock-root "$lock_root" \
    --worktree "$worktree" \
    --purpose "$purpose" \
    --evidence "$evidence" \
    --actions "$actions" \
    --forbidden "$forbidden" \
    -- "$script_path" "$@"
}

opena8dj_pid_alive() {
  local pid="$1"
  [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

opena8dj_cleanup_stale_lock() {
  # A dead PID is not proof that the kernel lock is free: an inherited child
  # may still hold it. The supervisor checks the stable flock inode instead.
  return 0
}

opena8dj_acquire_hardware_lock() {
  local worktree="$1" purpose="$2" evidence="$3" actions="$4" forbidden="$5"
  local lock_root
  lock_root="$(opena8dj_lock_root)"
  if ! OPENA8DJ_LOCK_ROOT="$lock_root" /usr/bin/python3 \
      "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/hardware-lock-supervisor.py" \
      --verify-inherited --lock-root "$lock_root"; then
    echo "audio_gate_lock=DENIED"
    echo "reason=kernel_lock_descriptor_not_inherited"
    return 75
  fi
  _OPENA8DJ_LOCK_ACQUIRED=0
  export OPENA8DJ_HARDWARE_LOCK_HELD=1
  : "$worktree" "$purpose" "$evidence" "$actions" "$forbidden"
}

opena8dj_release_hardware_lock() {
  # The supervisor owns the kernel lock and releases it after every inherited
  # mutator exits. A script must never unlink metadata or the lock inode.
  return 0
}
