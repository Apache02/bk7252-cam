---
name: wip-branch
description: Use when an implementation attempt has failed and you want to preserve knowledge before stopping work. Triggers on: "implementation failed", "park this branch", "dead end", "stop this direction".
---

# wip-branch

Preserve knowledge from a failed implementation attempt: annotate dead-end code,
commit to a `wip/` branch, surface learnings to main.

**Announce at start:** "Using wip-branch skill to preserve knowledge and commit the dead-end work."

## Step 1 — Gather context

Run:
```bash
git status
git diff
git log $(git merge-base HEAD main)..HEAD --oneline
```

Read only the files listed in `git status` to understand what was attempted.

## Step 2 — Analyze (three buckets)

Classify what is worth preserving:

**CODE** — annotations to add to changed files:
- Current state (working / partially working / not working at all)
- What was attempted (approach, sequence tried)
- What was observed on hardware (register values, behavior, timing)
- Unresolved hypotheses (what remains untested)

**DOCS** — new or updated `docs/hardware/<block>.md` content:
- Hardware findings that are factual and reusable, not yet documented elsewhere
- Skip if nothing new was discovered

**MAIN** — changes in files outside the core failing driver:
- Bug fixes in adjacent code discovered incidentally
- Corrections or additions that belong in existing docs
- Skip if nothing applies

Mark each bucket explicitly — if empty, write "nothing to surface."

## Step 3 — Propose branch name (conditional)

**If already on a `wip/*` branch: skip this step entirely.**

Otherwise, generate `wip/<topic>` where `<topic>` is derived from the failing
feature name. Present the proposed name and ask for confirmation before
proceeding. The `wip/` prefix is mandatory.

## Step 4 — Present plan

Show the full plan in three labelled sections. Do not execute anything until
the user approves.

```
[CODE ANNOTATIONS]
  <file> — <what will be added>
  ...

[BRANCH COMMIT]
  Branch: wip/<name>
  Message: wip(<name>): park — <one-line summary>

[MAIN CANDIDATES]
  <file> — <what and why>
  (or: nothing to surface)
```

## Step 5 — Execute (only after approval)

1. Write the code annotations into the changed files.
2. Handle the branch:
   - **Already on `wip/*`:** commit directly, no checkout.
   - **On another branch:** `git checkout -b wip/<name>` (or `git checkout wip/<name>`
     if the branch already exists), then commit.
3. If MAIN CANDIDATES is non-empty: present the list and ask the user separately
   what to transfer and how. Do not transfer anything automatically.

## Commit message format

```
wip(<branch-name>): park — <one-line summary>
```

Example: `wip(flash-bypass): park — XIP bypass via CMD register does not work`

## Rules

- Code annotations are **factual only**: observed values, tried sequences, confirmed
  behavior. No speculation.
- MAIN CANDIDATES are **proposed, never applied automatically**.
- If `git diff` is empty: say so and stop — there is nothing to preserve.
- Step 2 analysis is mandatory even when it seems obvious nothing is worth saving.

## Red Flags

Never:
- Execute any git operation before plan approval
- Switch to main or merge anything automatically
- Delete the branch
- Skip Step 2
