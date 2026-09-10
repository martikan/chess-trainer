# Git rules

- Write commit messages that explain **why**, not what — the diff shows the what
- Keep commits atomic — one logical change per commit
- Never push / force-push to `main` or `master`
- Never commit files larger than 5MB without explicit approval
- Stage specific files by name — avoid `git add .` or `git add -A` which can catch secrets
- Don't amend published commits — create a new commit instead
- Branch names: `feature/`, `fix/`, `chore/` prefixes with kebab-case descriptions
- Use Conventional Commits

