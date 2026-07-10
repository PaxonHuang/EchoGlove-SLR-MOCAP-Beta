---
description: Bootstrap/verify the Ubuntu x64 dev environment (conda + node + platformio + esp-idf + relay + web) in one shot
allowed-tools: Bash(!), Read
---

# /setup-env — EchoGlove one-shot dev bootstrap

Run the project's idempotent environment bootstrap script and report results.
The script is **detect-first**: it only installs what's missing and never
clobbers existing tools, so it's safe to re-run anytime.

## Steps

1. If `scripts/setup_env.sh` does not exist or is not executable, stop and tell the user.
2. Run with the user's intent (default: interactive for heavy stages; `--all` for full non-interactive install):

   ```bash
   ./scripts/setup_env.sh
   ```

   If `$ARGUMENTS` contains `all`, run `./scripts/setup_env.sh --all` instead.
   Pass through any other stage flags the user supplies (e.g. `--idf --relay`).

3. Stream the script output. When it finishes, read `docs/DEVELOPMENT_SETUP.md`
   and print a concise **"What to do next"** summary:
   - how to activate ESP-IDF (`source scripts/activate_idf.sh`)
   - how to run the relay (`conda run -n pytorch_env uvicorn ...`)
   - how to run the web dev server (`cd glove_web && npm run dev`)
   - how to build firmware (`pio run` / `idf.py build`)
   - the serial-permission reminder (log out/in for dialout group) if the script flagged it

## Notes
- Do NOT edit system config or install tools yourself — delegate everything to the script.
- If a stage fails, surface the script's `warn`/`✗` lines verbatim and suggest
  re-running just that stage, e.g. `./scripts/setup_env.sh --idf`.
- CPU-only by design (no CUDA). Training envs install `cpuonly` torch/tensorflow-cpu.
