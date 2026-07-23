#!/usr/bin/env bash
# =============================================================================
# EchoGlove-SLR-MOCAP — one-shot dev environment bootstrap (Ubuntu x64, CPU-only)
# =============================================================================
# Philosophy: detect-first, idempotent, optional-by-component.
#   - Never clobbers an existing install; only installs what's MISSING.
#   - Respects user-owned dirs (~/.nvm, ~/miniconda3, ~/esp/esp-idf).
#   - Each stage is independently skippable via env vars (see CONFIG below).
#
# Usage (from repo root):
#   ./scripts/setup_env.sh                 # interactive: prompt for heavy stages
#   ./scripts/setup_env.sh --all           # install everything, no prompts
#   ./scripts/setup_env.sh --relay --web   # pick specific stages
#   SKIP_IDF=1 ./scripts/setup_env.sh      # skip ESP-IDF (heaviest)
#
# Stages: base-os | conda | node | platformio | esp-idf | relay | web | verify
# =============================================================================
set -Eeuo pipefail

# ---------- pretty output ----------
C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YLW=$'\033[33m'; C_BLU=$'\033[34m'; C_RST=$'\033[0m'
log()  { printf "${C_BLU}▶${C_RST} %s\n" "$*"; }
ok()   { printf "${C_GRN}✓${C_RST} %s\n" "$*"; }
warn() { printf "${C_YLW}!${C_RST} %s\n" "$*"; }
die()  { printf "${C_RED}✗${C_RST} %s\n" "$*" >&2; exit 1; }

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# ---------- config (override via env) ----------
: "${SKIP_BASE_OS:=0}"
: "${SKIP_CONDA:=0}"
: "${SKIP_NODE:=0}"
: "${SKIP_PLATFORMIO:=0}"
: "${SKIP_IDF:=0}"          # ESP-IDF is the heaviest stage (~2GB) — opt-in friendly
: "${SKIP_RELAY:=0}"
: "${SKIP_WEB:=0}"
: "${SKIP_VERIFY:=0}"

# ESP-IDF pin (P4 BSP requires idf>=5.4; project verified against 5.4)
: "${IDF_VERSION:=v5.4}"
: "${IDF_DIR:=$HOME/esp/esp-idf}"
: "${IDF_TOOLS_DIR:=$HOME/esp/esp-idf-tools}"   # separate from IDF_DIR to allow shared cache
: "${CONDA_DIR:=$HOME/miniconda3}"
: "${NODE_LTS:=22}"          # project uses Vite 6 + React 18; Node 22 LTS

# ---------- arg parsing ----------
INSTALL_ALL=0
EXPLICIT=0
SELECTED=()
while [ $# -gt 0 ]; do
  case "$1" in
    --all)        INSTALL_ALL=1 ;;
    --base-os)    SELECTED+=("base-os") ;;
    --conda)      SELECTED+=("conda") ;;
    --node)       SELECTED+=("node") ;;
    --platformio) SELECTED+=("platformio") ;;
    --idf)        SELECTED+=("idf") ;;
    --relay)      SELECTED+=("relay") ;;
    --web)        SELECTED+=("web") ;;
    --verify)     SELECTED+=("verify") ;;
    -h|--help)
      sed -n '2,20p' "$0"; exit 0 ;;
    *) die "Unknown arg: $1 (try --help)" ;;
  esac
  shift
done

# Mode resolution: if --all or explicit stages given, run non-interactive.
# Otherwise prompt for the heavy stages (IDF), auto-run the light ones.
if [ "$INSTALL_ALL" = "1" ]; then
  AUTO_YES=1
elif [ "${#SELECTED[@]}" -gt 0 ]; then
  AUTO_YES=1
else
  AUTO_YES=0
fi
want() {  # want <stage-name>
  if [ "$INSTALL_ALL" = "1" ]; then return 0; fi
  if [ "${#SELECTED[@]}" -gt 0 ]; then
    for s in "${SELECTED[@]}"; do [ "$s" = "$1" ] && return 0; done
    return 1
  fi
  return 0   # default interactive mode: include unless SKIP_<X>=1
}
skipped() {  # skipped <STAGE> -> echo 0/1
  local s="$1"; local var="SKIP_${s^^}"; [ "${!var:-0}" = "1" ]
}

# ---------- yes/no prompt for heavy optional stages ----------
confirm() {  # confirm <prompt>  -> exit 0 if yes
  if [ "$AUTO_YES" = "1" ]; then return 0; fi
  read -r -p "$(printf "${C_YLW}?${C_RST} %s [y/N] " "$1")" ans
  [[ "$ans" =~ ^[Yy]$ ]]
}

# =============================================================================
# STAGE 1: base OS packages
# =============================================================================
stage_base_os() {
  log "Checking OS dependencies"
  command -v apt-get >/dev/null || { warn "apt-get not found — skipping OS stage (non-Debian?)"; return; }
  local pkgs=(git curl wget python3 python3-pip python3-venv build-essential cmake ninja-build pkg-config
              libusb-1.0-0-dev udev ca-certificates)
  local missing=()
  for p in "${pkgs[@]}"; do
    dpkg -s "$p" >/dev/null 2>&1 || missing+=("$p")
  done
  if [ "${#missing[@]}" -eq 0 ]; then ok "All OS packages present"; return; fi
  log "Installing: ${missing[*]}"
  sudo apt-get update -qq
  sudo apt-get install -y --no-install-recommends "${missing[@]}"
  # serial device access for PlatformIO / idf.py flashes
  if ! getent group dialout | grep -q "$USER" 2>/dev/null; then
    sudo usermod -aG dialout "$USER" || true
    warn "Added $USER to 'dialout' group — LOG OUT/IN (or reboot) for serial access to take effect."
  fi
  ok "OS dependencies installed"
}

# =============================================================================
# STAGE 2: miniconda + conda envs (pytorch_env, tf_env)
# =============================================================================
stage_conda() {
  log "Checking conda"
  if command -v conda >/dev/null 2>&1; then
    ok "conda already installed: $(conda --version)"
    CONDA_BASE="$(conda info --base)"
  else
    if ! confirm "miniconda not found. Install to $CONDA_DIR? (~500MB)"; then
      warn "Skipping conda — relay/training envs will not be set up."; return
    fi
    log "Installing miniconda to $CONDA_DIR"
    local tmp; tmp="$(mktemp -d)"; local inst="$tmp/Miniconda.sh"
    wget -q -O "$inst" "https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh"
    bash "$inst" -b -p "$CONDA_DIR"
    rm -rf "$tmp"
    CONDA_BASE="$CONDA_DIR"
    # activate for this shell
    # shellcheck disable=SC1091
    source "$CONDA_BASE/etc/profile.d/conda.sh"
    ok "miniconda installed"
  fi
  # shellcheck disable=SC1091
  [ -n "${CONDA_BASE:-}" ] && source "$CONDA_BASE/etc/profile.d/conda.sh" 2>/dev/null || true

  for envfile in glove_relay/environment.yml glove_relay/environment_tf.yml; do
    [ -f "$envfile" ] || { warn "$envfile missing — skipping"; continue; }
    local envname; envname="$(grep -m1 '^name:' "$envfile" | awk '{print $2}')"
    if conda env list | awk '{print $1}' | grep -qx "$envname"; then
      log "Updating existing conda env: $envname"
      conda env update -f "$envfile" --prune || warn "env update had issues for $envname"
    else
      log "Creating conda env: $envname (from $envfile)"
      conda env create -f "$envfile" || warn "env create had issues for $envname"
    fi
    ok "conda env ready: $envname"
  done
}

# =============================================================================
# STAGE 3: nvm + Node LTS
# =============================================================================
stage_node() {
  log "Checking Node.js"
  if [ -s "$HOME/.nvm/nvm.sh" ]; then
    # shellcheck disable=SC1091
    source "$HOME/.nvm/nvm.sh"
    ok "nvm present ($(nvm --version 2>/dev/null))"
  else
    if ! confirm "nvm not found. Install nvm + Node $NODE_LTS LTS?"; then
      warn "Skipping Node — web frontend (glove_web) will not build."; return
    fi
    log "Installing nvm"
    wget -qO- "https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh" | bash
    # shellcheck disable=SC1091
    source "$HOME/.nvm/nvm.sh"
    ok "nvm installed"
  fi
  if ! nvm version "$NODE_LTS" >/dev/null 2>&1; then
    log "Installing Node $NODE_LTS (LTS)"
    nvm install "$NODE_LTS"
  fi
  nvm use "$NODE_LTS" >/dev/null 2>&1 || true
  nvm alias default "$NODE_LTS" >/dev/null 2>&1 || true
  ok "Node $(node --version) active (via nvm, default $NODE_LTS)"
}

# =============================================================================
# STAGE 4: PlatformIO (ESP32-S3 gloves)
# =============================================================================
stage_platformio() {
  log "Checking PlatformIO"
  if command -v pio >/dev/null 2>&1; then ok "PlatformIO present: $(pio --version 2>/dev/null | tr -d '\n')"; return; fi
  # Prefer standalone pipx-style install to avoid polluting system python
  log "Installing PlatformIO Core"
  python3 -m pip install --user platformio || die "PlatformIO install failed"
  # ensure ~/.local/bin on PATH this session
  case ":$PATH:" in *":$HOME/.local/bin:"*) ;; *) export PATH="$HOME/.local/bin:$PATH";; esac
  command -v pio >/dev/null 2>&1 || die "pio not on PATH after install — add ~/.local/bin to PATH"
  ok "PlatformIO installed: $(pio --version 2>/dev/null | tr -d '\n')"
}

# =============================================================================
# STAGE 5: ESP-IDF (P4/C6 — heaviest stage, ~2GB)
# =============================================================================
stage_idf() {
  log "Checking ESP-IDF"
  if [ -f "$IDF_DIR/export.sh" ] && grep -qx "$IDF_VERSION" "$IDF_DIR/version.txt" 2>/dev/null; then
    ok "ESP-IDF $IDF_VERSION present at $IDF_DIR"
  elif [ -f "$IDF_DIR/export.sh" ]; then
    warn "ESP-IDF found at $IDF_DIR but version != $IDF_VERSION ($(cat "$IDF_DIR/version.txt" 2>/dev/null || echo unknown))"
    if confirm "Checkout $IDF_VERSION and reinstall tools?"; then
      ( cd "$IDF_DIR" && git fetch --tags -q && git checkout "$IDF_VERSION" -q && git submodule update --init --recursive -q )
      ( cd "$IDF_DIR" && IDF_TOOLS_PATH="$IDF_TOOLS_DIR" ./install.sh all ) || warn "IDF install.sh reported issues"
    else warn "Keeping existing ESP-IDF as-is"; fi
  else
    if ! confirm "ESP-IDF not found. Clone $IDF_VERSION to $IDF_DIR + install tools (~2GB)?"; then
      warn "Skipping ESP-IDF — P4/C6 firmware builds will be unavailable."; return
    fi
    log "Cloning ESP-IDF $IDF_VERSION"
    mkdir -p "$(dirname "$IDF_DIR")"
    git clone -b "$IDF_VERSION" --recursive https://github.com/espressif/esp-idf.git "$IDF_DIR"
    ( cd "$IDF_DIR" && IDF_TOOLS_PATH="$IDF_TOOLS_DIR" ./install.sh all ) || die "IDF install.sh failed"
  fi
  # Drop an activation helper the user can source in any shell.
  local helper="$REPO_ROOT/scripts/activate_idf.sh"
  cat > "$helper" <<EOF
#!/usr/bin/env bash
# Source this to activate ESP-IDF in any shell:  source scripts/activate_idf.sh
export IDF_PATH="$IDF_DIR"
export IDF_TOOLS_PATH="$IDF_TOOLS_DIR"
# shellcheck disable=SC1091
source "$IDF_DIR/export.sh" >/dev/null 2>&1 || { echo "Failed to activate ESP-IDF from $IDF_DIR"; return 1; }
echo "ESP-IDF \$(idf.py --version 2>/dev/null) activated (IDF_TOOLS_PATH=$IDF_TOOLS_DIR)"
EOF
  chmod +x "$helper"
  ok "ESP-IDF ready. Activate per-shell with:  source scripts/activate_idf.sh"
}

# =============================================================================
# STAGE 6: relay deps (ensure pytorch_env has relay requirements)
# =============================================================================
stage_relay() {
  log "Verifying relay environment (pytorch_env)"
  if ! command -v conda >/dev/null 2>&1; then warn "conda missing — run --conda first"; return; fi
  # ensure conda in this shell
  # shellcheck disable=SC1091
  [ -s "$HOME/miniconda3/etc/profile.d/conda.sh" ] && source "$HOME/miniconda3/etc/profile.d/conda.sh" 2>/dev/null || true
  [ -s "$CONDA_DIR/etc/profile.d/conda.sh" ] && source "$CONDA_DIR/etc/profile.d/conda.sh" 2>/dev/null || true
  if ! conda env list | awk '{print $1}' | grep -qx pytorch_env; then
    warn "pytorch_env missing — conda stage may have been skipped. Creating from environment.yml"
    conda env create -f glove_relay/environment.yml || { warn "Could not create pytorch_env"; return; }
  fi
  ok "relay env (pytorch_env) ready. Run relay with:  conda run -n pytorch_env uvicorn glove_relay.src.main:app --port 8000"
}

# =============================================================================
# STAGE 7: web deps (npm install in glove_web)
# =============================================================================
stage_web() {
  log "Installing web frontend deps (glove_web)"
  if [ ! -d glove_web ]; then warn "glove_web not found"; return; fi
  # ensure nvm loaded
  [ -s "$HOME/.nvm/nvm.sh" ] && { # shellcheck disable=SC1091
    source "$HOME/.nvm/nvm.sh"; nvm use "$NODE_LTS" >/dev/null 2>&1 || true; }
  ( cd glove_web && [ -d node_modules ] && { ok "node_modules exists, skipping npm install"; } || { npm install; } )
  ok "web deps ready. Run dev server with:  cd glove_web && npm run dev"
}

# =============================================================================
# STAGE 8: verification (smoke tests — does each tool respond?)
# =============================================================================
stage_verify() {
  log "Running verification smoke checks"
  local pass=0 fail=0
  chk() { if "$@" >/dev/null 2>&1; then ok "  $1"; pass=$((pass+1)); else warn "  $1 NOT available"; fail=$((fail+1)); fi; }
  chk git --version
  chk python3 --version
  command -v conda >/dev/null && { conda env list | grep -q pytorch_env && ok "  conda:pytorch_env" || warn "  conda:pytorch_env NOT created"; }
  command -v node >/dev/null && chk node --version
  command -v pio >/dev/null && chk pio --version
  [ -f "$IDF_DIR/export.sh" ] && ok "  esp-idf:present($IDF_DIR)" || warn "  esp-idf:missing (use --idf)"
  [ -d glove_web/node_modules ] && ok "  glove_web:node_modules" || warn "  glove_web:node_modules missing (use --web)"
  printf "\n${C_BLU}Verification:${C_RST} %s passed, %s missing\n" "$pass" "$fail"
  [ "$fail" -eq 0 ] && ok "Environment ready 🎉" || warn "Some optional components absent — re-run with the relevant --<stage> flag."
}

# =============================================================================
# RUN
# =============================================================================
printf "${C_BLU}=== EchoGlove dev environment bootstrap ===${C_RST}\n"
run_stage() { local name="$1" skip_var="$2"; local fn="stage_${name//-/_}";
  if skipped "$skip_var"; then warn "[$name] skipped (SKIP_$skip_var=1)"; return 0; fi
  want "$name" || return 0
  "$fn"; }
run_stage base-os    BASE_OS
run_stage conda      CONDA
run_stage node       NODE
run_stage platformio PLATFORMIO
run_stage idf        IDF
run_stage relay      RELAY
run_stage web        WEB
[ "$SKIP_VERIFY" = "1" ] || { want verify && stage_verify; }
printf "${C_BLU}=== done ===${C_RST}\n"
