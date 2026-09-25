#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOOKS_DIR="$REPO_ROOT/.githooks"

if [[ ! -d "$HOOKS_DIR" ]]; then
  echo "ERROR: hooks directory not found: $HOOKS_DIR"
  exit 1
fi

chmod +x "$HOOKS_DIR/pre-push"
git config --local core.hooksPath .githooks

# Driver "ours" per .gitattributes: nei merge da upstream (Tahoma2D) i file
# delle regole per gli agenti restano sempre quelli di Ztoryc.
git config --local merge.ours.driver true

if ! command -v scanoss-py >/dev/null 2>&1; then
  echo "NOTE: scanoss-py non installato: il pre-push saltera' la scansione licenze."
  echo "      Installare con: pip install scanoss"
fi

echo "Installed repository git safety hooks."
echo "Active hooksPath: $(git config --local core.hooksPath)"
