---
name: commit-changelist
description: Use when asked to commit a named JetBrains IDE changelist or stage files from a specific changelist. Reads .idea/workspace.xml to find which files belong to that changelist.
---

# commit-changelist

Commit one JetBrains IDE changelist as a single atomic git commit — reads file assignments directly from `.idea/workspace.xml`.

**Announce at start:** "Using commit-changelist skill to stage and commit the '<name>' changelist."

## Script

All operations go through:
```
.claude/skills/commit-changelist/changelist.py
```

Available modes:
```sh
python3 .claude/skills/commit-changelist/changelist.py               # list all changelists
python3 .claude/skills/commit-changelist/changelist.py <name>        # show files
python3 .claude/skills/commit-changelist/changelist.py <name> --comment   # print stored draft message
python3 .claude/skills/commit-changelist/changelist.py <name> --stage     # git add/rm those files
```

## Steps

### Step 1 — Inspect

Run without flags to show files:
```sh
python3 .claude/skills/commit-changelist/changelist.py <name>
```

If changelist is empty or not found — stop and report.

### Step 2 — Propose commit message

Check the stored comment:
```sh
python3 .claude/skills/commit-changelist/changelist.py <name> --comment
```

The `comment` field is editable from the JetBrains IDE commit window — the user may have typed a draft there. But the IDE also auto-fills it with the last commit message when the changelist becomes active, so it is often stale.

- If non-empty: show it as a suggestion, but always ask the user whether to use it or draft a new one from the file list.
- If empty: draft a conventional commit message from the file list (`feat`/`fix`/`chore`/`refactor`, scope derived from changelist name or changed paths).

Present the proposed message and file list to the user. **Do not proceed without explicit approval.**

### Step 3 — Stage

After approval:
```sh
python3 .claude/skills/commit-changelist/changelist.py <name> --stage
```

`--stage` reconciles the git staging area with the changelist:
- unstages any files currently staged that do not belong to this changelist
- stages all changelist files in full (picks up both previously staged and unstaged changes)

Verify with `git status` that only the expected files are staged.

### Step 4 — Commit

```sh
git commit -m "$(cat <<'EOF'
<approved message>

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

## workspace.xml format (reference)

```xml
<component name="ChangeListManager">
  <list default="true" id="..." name="Changes" comment="feat: ...">
    <change beforePath="$PROJECT_DIR$/src/foo.c" afterPath="$PROJECT_DIR$/src/foo.c" />
  </list>
  <list id="..." name="wifi" comment="">
    <!-- new file: afterPath only -->
    <change afterPath="$PROJECT_DIR$/src/platform/port_wifi/CMakeLists.txt" afterDir="false" />
    <!-- deleted file: beforePath only -->
    <change beforePath="$PROJECT_DIR$/src/old.c" beforeDir="false" />
  </list>
</component>
```

## Rules

- Never stage or commit before the user approves the file list and message.
- Never touch files from other changelists.
- If `git status` after `--stage` shows unexpected files already staged, stop and report — do not commit.
- `Co-Authored-By` trailer is always appended.
