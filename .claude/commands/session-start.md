# Session Start

Initialize a new Claude Code session for Flow5 development.

## Instructions

Run these steps to prepare for work:

### 1. Set Git Identity
```bash
export GIT_AUTHOR_NAME="Claude Agent" GIT_AUTHOR_EMAIL="claude@agent.flow5" GIT_COMMITTER_NAME="Claude Agent" GIT_COMMITTER_EMAIL="claude@agent.flow5"
```

### 2. Sync with Remote
```bash
cd /home/johe2/optiflow5 && git fetch origin && git status
```

### 3. Check for Unfinished Work
```bash
cd /home/johe2/optiflow5 && git stash list
```

### 4. Show Available Issues
```bash
cd /home/johe2/optiflow5 && bd ready
```

### 5. Quick Build Check
```bash
cd /home/johe2/optiflow5 && make -j4 2>&1 | tail -5
```

## Output Summary

Provide a session status report:

```
SESSION INITIALIZED
===================
Git Identity:  Claude Agent <claude@agent.flow5>
Branch:        [current branch]
Status:        [clean/dirty/ahead/behind]
Stashes:       [count]
Ready Issues:  [list from bd ready]
Build:         [OK/NEEDS REBUILD]

Suggested next step: [based on status]
```

## Suggested Next Steps

Based on the status, suggest ONE of:
- "Run `/build` to rebuild after pulling changes"
- "Run `bd show <id>` to review the top priority issue"
- "Continue work on uncommitted changes"
- "Run `/ci-check` before pushing unpushed commits"
