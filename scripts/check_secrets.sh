#!/usr/bin/env bash
# Fails (exit 1) if any quoted value from secrets.h is found in the git index,
# or in the whole history with --history. Never prints the values themselves.
# Only password macros (*PASS*) are checked: user names are short, generic
# strings (e.g. "admin") that would match everywhere and are not secret.
cd "$(git rev-parse --show-toplevel)" || exit 2
f=""
for c in secrets.h include/secrets.h; do [ -f "$c" ] && f="$c" && break; done
if [ -z "$f" ]; then echo "check_secrets: no secrets.h found, nothing to check"; exit 0; fi
hist=0; [ "$1" = "--history" ] && hist=1
bad=0
while IFS= read -r v; do
  [ -z "$v" ] && continue
  if [ $hist -eq 1 ]; then
    revs=$(git rev-list --all) || { echo "check_secrets: ERROR: git rev-list failed" >&2; exit 2; }
    [ -z "$revs" ] && continue
    # shellcheck disable=SC2086
    files=$(git grep -l -F -e "$v" $revs 2>/dev/null)
  else
    files=$(git grep --cached -l -F -e "$v" 2>/dev/null)
  fi
  rc=$?
  # git grep: 0 = match, 1 = no match, >1 = error (must not pass silently)
  if [ $rc -gt 1 ]; then
    echo "check_secrets: ERROR: git grep failed (exit $rc); result is unreliable" >&2
    exit 2
  fi
  if [ -n "$files" ]; then
    echo "LEAK: secret value (length ${#v}) found in:"
    echo "$files" | sed 's/^/  /' | sort -u
    bad=1
  fi
# Only *PASS* macros are scanned on purpose: user names are not secrets and
# cause false positives.
done < <(grep -E '^[[:space:]]*#define[[:space:]]+[A-Z_]*PASS' "$f" | grep -oE '"[^"]*"' | sed -E 's/^"(.*)"$/\1/')
if [ $bad -eq 1 ]; then exit 1; fi
echo "check_secrets: OK"
exit 0
