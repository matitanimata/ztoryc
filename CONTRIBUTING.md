# Contributing to Ztoryc

> 💬 **Talk to us first if you like:** the [Ztoryc Discord](https://discord.gg/FcjPJnkqv) is the quickest
> way to ask whether an idea fits, get pointed at the right file, or report something
> odd before writing it up as an issue.


Ztoryc is currently in active development by Matitanimata.

## Bug reports

If you find a bug, please open a [GitHub issue](https://github.com/matitanimata/ztoryc/issues)
with as much detail as possible: operating system, steps to reproduce, screenshots or video.

## Pull requests

Contributions are welcome. If you fixed or improved something, open a pull request.
We will review it and either accept it, request changes, or decline with an explanation.

### Default target repository (important)

- Open pull requests to **https://github.com/matitanimata/ztoryc**.
- Base branch is **`master`** (not `main`).
- Treat this as the default for all feature and bug-fix work.

### Workflow

1. Fork the repository on GitHub.
2. Clone your fork and create a branch:
git checkout -b fix/your-fix-name 
3. Make your changes and test them.
4. Apply clang-format using `toonz/sources/.clang-format`:
cd toonz/sources
./beautification.sh
5. Commit with a clear message and push your branch.
6. Open a pull request against the `master` branch in `matitanimata/ztoryc`.

### Git safety bootstrap (run once per clone)

Run these commands after cloning to reduce the risk of accidental upstream PRs/pushes:

```bash
gh repo set-default matitanimata/ztoryc
git remote set-url --push upstream DISABLED 2>/dev/null || true
bash scripts/install-git-safety.sh
```

What this does:
- Forces `gh pr create` default target to `matitanimata/ztoryc`.
- Disables accidental pushes to `upstream` if that remote exists.
- Installs a local `pre-push` hook that blocks pushes to remotes other than `origin`.

## Upstream contributions

Ztoryc is a fork of [Tahoma2D](https://github.com/tahoma2d/tahoma2d).
If your fix is relevant to the base engine and not Ztoryc-specific,
consider also submitting it upstream to Tahoma2D or OpenToonz.

### Upstream PR policy

- Do **not** open upstream PRs by default.
- Open an upstream PR only when a maintainer explicitly requests it.
- Upstream PRs should usually come **after** the Ztoryc PR is reviewed and merged.

## Translations

Translation `.ts` files are in `toonz/sources/translations`.
Use [Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html) to edit them.
Submit updated `.ts` and generated `.qm` files via pull request.
