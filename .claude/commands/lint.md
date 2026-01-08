# Lint Headers

Run header lint script on changed files.

## Instructions

Check for staged changes first:
```bash
cd /home/johe2/optiflow5 && git diff --cached --name-only
```

If there are staged files, run:
```bash
cd /home/johe2/optiflow5 && scripts/lint_headers.sh HEAD~1
```

Report any header issues found (missing copyright, wrong author for new files, etc.)
