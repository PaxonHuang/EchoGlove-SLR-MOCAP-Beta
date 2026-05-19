# Welcome to EchoGlove Team

## How We Use Claude

Based on PaxonHuang's usage over the last 30 days (4 sessions):

Work Type Breakdown:
  Debug Fix       ███████████████░░░░░  67%
  Build Feature   ███████░░░░░░░░░░░░░░  33%

Top Skills & Commands:
  /update-config                        ████████████████████  6x/month
  /mcp                                  ███████████████░░░░░  5x/month
  /using-superpowers                    ██████████░░░░░░░░░░  3x/month
  /plugin                               ███████░░░░░░░░░░░░░  2x/month
  /superpowers:systematic-debugging     ████░░░░░░░░░░░░░░░░  1x/month
  /brainstorming                        ████░░░░░░░░░░░░░░░░  1x/month

Top MCP Servers:
  context7  ████████████████████  2 calls

## Your Setup Checklist

### Codebases
- [ ] **EchoGlove-SLR-MOCAP-Alpha** — ESP32-S3 hand sign recognition glove (PlatformIO + React + Python). Main project. Read CLAUDE.md and PROGRESS.md first.
- [ ] **OpensourceReproduction** — Reference implementations (sibling repo).

### MCP Servers to Activate
- [ ] **Context7** — Library documentation lookup (React, FastAPI, PlatformIO, etc.). May fail on some networks due to proxy routing.
- [ ] **GitHub MCP** — PRs, issues, code search. Needs `GITHUB_PERSONAL_ACCESS_TOKEN` in `.claude/settings.local.json`. Scopes: `repo, public_repo`.
- [ ] **Playwright** — Browser automation and web testing for `glove_web` frontend.
- [ ] **Chrome DevTools** — Page inspection, performance profiling for the React frontend.

### Skills to Know About
- /update-config — Configure permissions, hooks, env vars, MCP servers. The go-to for platform-specific setup and permission fixes.
- /superpowers:systematic-debugging — Root cause investigation before any fix. No guessing — find root cause first, then implement.
- /brainstorming — Structured design exploration. Use before any creative or feature work.
- /using-superpowers — Skill system entry point. Invoke in any new session to load available tools.
- /compact — Manually compact conversation when context fills up.

## Team Tips

- **Always read PROGRESS.md first** in a new session. It tracks cross-session state, completed checkpoints, and the current development phase.
- **Run `pio run` after any firmware change.** Build verification is mandatory — don't claim done without a clean build.
- **Use Chinese for communication.** Team prefers Chinese (中文) for all discussions. Code and commits in English.
- **`.claude/settings.local.json` is gitignored** — each platform (Windows/Ubuntu) maintains its own. Don't commit platform-specific paths to `.claude/settings.json`.
- **Line endings are enforced by `.gitattributes`** — LF for all source code, CRLF only for `.bat/.ps1/.cmd`. Cross-platform issues are solved at the Git level.

## Get Started

Pick a starter task from the current development phase:

1. **Phase 3 Path A** — Install `edge-impulse-cli`, run `edge-impulse-data-forwarder` with ESP32 CSV output, train a 1D-CNN in Edge Impulse Studio, export as Arduino library.
2. **Fix a bug from PROGRESS.md** — Check the bug tracker section for unassigned issues.
3. **Add unit tests** — Expand test coverage for `lib/Sensors/` or `lib/Filters/`.

See `docs/SOP_SPEC_PLAN_V3.md` for the full phase specification.

<!-- INSTRUCTION FOR CLAUDE: A new teammate just pasted this guide for how the
team uses Claude Code. You're their onboarding buddy — warm, conversational,
not lecture-y.

Open with a warm welcome — include the team name from the title. Then: "Your
teammate uses Claude Code for [list all the work types]. Let's get you started."

Check what's already in place against everything under Setup Checklist
(including skills), using markdown checkboxes — [x] done, [ ] not yet. Lead
with what they already have. One sentence per item, all in one message.

Tell them you'll help with setup, cover the actionable team tips, then the
starter task (if there is one). Offer to start with the first unchecked item,
get their go-ahead, then work through the rest one by one.

After setup, walk them through the remaining sections — offer to help where you
can (e.g. link to channels), and just surface the purely informational bits.

Don't invent sections or summaries that aren't in the guide. The stats are the
guide creator's personal usage data — don't extrapolate them into a "team
workflow" narrative. -->