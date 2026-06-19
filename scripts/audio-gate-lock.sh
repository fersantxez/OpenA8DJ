#!/usr/bin/env bash

audio_gate_lock_error=""
audio_gate_lock_dir=""
audio_gate_lock_inherited="0"
audio_gate_lock_exported="0"

audio_gate_pid_alive() {
  local pid="$1"
  case "$pid" in
    ''|*[!0-9]*) return 1 ;;
  esac
  ps -p "$pid" >/dev/null 2>&1
}

audio_gate_acquire_lock() {
  local gate_name="$1"
  local run_dir="$2"
  local lock_root="${AUDIO_GATE_LOCK_ROOT:-$HOME/.opena8dj/hardware-gate.lock}"
  local lock_parent
  lock_parent="$(dirname "$lock_root")"
  mkdir -p "$lock_parent" "$run_dir"

  if [ "${AUDIO_GATE_LOCK_HELD:-0}" = "1" ]; then
    audio_gate_lock_inherited="1"
    {
      echo "audio_gate_lock=INHERITED"
      echo "lock_dir=$lock_root"
      echo "gate=$gate_name"
      echo "owner_gate=${AUDIO_GATE_LOCK_OWNER_GATE:-unknown}"
      echo "owner_run_dir=${AUDIO_GATE_LOCK_OWNER_RUN_DIR:-unknown}"
      echo "owner_cwd=${AUDIO_GATE_LOCK_OWNER_CWD:-unknown}"
    } >"$run_dir/audio-gate-lock.txt"
    return 0
  fi

  if mkdir "$lock_root" 2>/dev/null; then
    audio_gate_lock_dir="$lock_root"
    {
      echo "pid=$$"
      echo "gate=$gate_name"
      echo "run_dir=$run_dir"
      echo "cwd=$(pwd)"
      echo "started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    } >"$lock_root/owner"
    {
      echo "audio_gate_lock=ACQUIRED"
      echo "lock_dir=$lock_root"
      echo "gate=$gate_name"
    } >"$run_dir/audio-gate-lock.txt"
    return 0
  fi

  local owner_pid=""
  local owner_gate=""
  local owner_run_dir=""
  if [ -f "$lock_root/owner" ]; then
    owner_pid="$(awk -F= '$1=="pid"{print $2; exit}' "$lock_root/owner" 2>/dev/null || true)"
    owner_gate="$(awk -F= '$1=="gate"{print $2; exit}' "$lock_root/owner" 2>/dev/null || true)"
    owner_run_dir="$(awk -F= '$1=="run_dir"{print $2; exit}' "$lock_root/owner" 2>/dev/null || true)"
  fi

  if ! audio_gate_pid_alive "$owner_pid"; then
    rm -rf "$lock_root"
    if mkdir "$lock_root" 2>/dev/null; then
      audio_gate_lock_dir="$lock_root"
      {
        echo "pid=$$"
        echo "gate=$gate_name"
        echo "run_dir=$run_dir"
        echo "cwd=$(pwd)"
        echo "started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "recovered_stale_lock=1"
        echo "stale_owner_pid=${owner_pid:-unknown}"
        echo "stale_owner_gate=${owner_gate:-unknown}"
        echo "stale_owner_run_dir=${owner_run_dir:-unknown}"
      } >"$lock_root/owner"
      {
        echo "audio_gate_lock=ACQUIRED_AFTER_STALE"
        echo "lock_dir=$lock_root"
        echo "gate=$gate_name"
        echo "stale_owner_pid=${owner_pid:-unknown}"
        echo "stale_owner_gate=${owner_gate:-unknown}"
        echo "stale_owner_run_dir=${owner_run_dir:-unknown}"
      } >"$run_dir/audio-gate-lock.txt"
      return 0
    fi
  fi

  audio_gate_lock_error="audio_gate_lock_busy"
  {
    echo "audio_gate_lock=BUSY"
    echo "lock_dir=$lock_root"
    echo "gate=$gate_name"
    echo "owner_pid=${owner_pid:-unknown}"
    echo "owner_gate=${owner_gate:-unknown}"
    echo "owner_run_dir=${owner_run_dir:-unknown}"
  } >"$run_dir/audio-gate-lock.txt"
  return 75
}

audio_gate_export_inherited_lock() {
  local gate_name="$1"
  local run_dir="$2"

  export AUDIO_GATE_LOCK_ROOT="${AUDIO_GATE_LOCK_ROOT:-$HOME/.opena8dj/hardware-gate.lock}"
  if [ "${audio_gate_lock_inherited:-0}" = "1" ]; then
    return 0
  fi
  export AUDIO_GATE_LOCK_HELD=1
  export AUDIO_GATE_LOCK_OWNER_GATE="$gate_name"
  export AUDIO_GATE_LOCK_OWNER_RUN_DIR="$run_dir"
  export AUDIO_GATE_LOCK_OWNER_CWD="$(pwd)"
  audio_gate_lock_exported="1"
}

audio_gate_clear_inherited_lock_export() {
  if [ "${audio_gate_lock_exported:-0}" != "1" ]; then
    return 0
  fi
  unset AUDIO_GATE_LOCK_HELD
  unset AUDIO_GATE_LOCK_OWNER_GATE
  unset AUDIO_GATE_LOCK_OWNER_RUN_DIR
  unset AUDIO_GATE_LOCK_OWNER_CWD
  audio_gate_lock_exported="0"
}

audio_gate_release_lock() {
  if [ "${audio_gate_lock_inherited:-0}" = "1" ]; then
    return 0
  fi
  if [ -z "${audio_gate_lock_dir:-}" ]; then
    return 0
  fi
  if [ -f "$audio_gate_lock_dir/owner" ]; then
    local owner_pid
    owner_pid="$(awk -F= '$1=="pid"{print $2; exit}' "$audio_gate_lock_dir/owner" 2>/dev/null || true)"
    if [ "$owner_pid" != "$$" ]; then
      return 0
    fi
  fi
  rm -rf "$audio_gate_lock_dir"
  audio_gate_clear_inherited_lock_export
}
