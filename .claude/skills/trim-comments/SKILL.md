---
name: trim-comments
description: Use when comments in this repo have grown too long or too chatty and need cutting back — triggers on "shorten the comments", "comments got bloated", "trim the comments in <file>", "clean up comments", or when a review notes a comment restates the code, defends the code, or reads as a note-to-self.
---

# trim-comments

Cut comments down to the non-obvious facts only. Code is never touched.

**Announce at start:** "Using trim-comments skill to shorten comments in <target>."

## Scope

Ask for a target if none was given: one file or one directory at a time. A wider
sweep produces a diff nobody can review.

## Rules

Apply these to every comment in the target.

**Length:** one or two short sentences, maximum. If it runs past two lines, cut
before you keep.

**Keep** only what the reader cannot get from reading the code — a genuine
non-obvious WHY, stated as a fact.

**Delete outright:**

- Wordy justifications defending why the code is written the way it is.
- Notes-to-self: "be careful", "if this ever changes, do Y", "worth watching",
  "this stays valid only as long as...".
- Restating the code in prose.
- Repeating the function or variable name.
- Truisms and decoration.

**Never touch:**

- `TODO` / `FIXME` / `XXX` markers — work markers, not comments.
- In `src/platform/port_wifi/` only, any comment line starting `imported: `.
  These are service markers naming the vendor objects that import a symbol, e.g.
  `// imported: libip(rwnx.o, sm.o)`. Skip them entirely: copy the line through
  verbatim, do not shorten the object list, and do not count it against the
  length rule. A short note after a dash on the same line is part of the marker.
- Doc comments on public API.
- Register names, bit numbers, datasheet and vendor-SDK references.
- Any line of code. Comments only, no refactoring rides along.

## Explanation vs. justification

The dividing line, and the one that gets missed most often.

An explanation states a fact:

```c
/* Reading clears the flag, so the value must be cached before the second read. */
```

A justification argues with the reader — delete it:

```c
/* This may look redundant, but it is actually necessary, because if we did not
   cache it here then a later read would return zero, and while that might seem
   acceptable it would break the caller, so the extra variable is worth it. */
```

Same fact, one sentence, no defence.

## Output

Show the diff. Do not build, do not run anything, do not commit.

## Common mistakes

| Mistake | Fix |
|---|---|
| Cutting 20% because each comment "seems justified" | Justified still means one to two sentences. Length is the rule, not merit. |
| Deleting a `TODO`, or shortening an `imported:` list | A marker is data. Copy it through untouched. |
| Treating `imported:` as a rule outside `port_wifi` | It is a marker only there. Elsewhere it is an ordinary comment. |
| Rewording code alongside the comment | Comments only. A code change belongs in its own diff. |
| Compressing a long comment into a dense one | Cut content, do not compress prose. |
