# Build and release scripts

## macOS

| Script | Purpose | Output |
| --- | --- | --- |
| `requisites-install.sh` | Installs the complete INDI development dependency set. | Homebrew dependencies and Google Test. |
| `indi-core-build.sh [Debug\|Release]` | Builds the full INDI core, including tests and the Qt client. | `build/indi-core` |
| `indi-core-test.sh` | Runs the full-core integration and unit tests. | Test results |
| `indi-core-package-build.sh` | Packages the full-core install tree. | `packages/indi-core-package.tar` |
| `update-homebrew-formula.sh <tag> [formula]` | Pins a formula to an INDI release and computes its archive checksum. | Updated formula |

Use the `indi-core-*` scripts for full INDI development and test coverage. The
server-only distribution path is the Homebrew formula, which builds and installs
the `indiserver` command without bundled drivers.

The macOS GitHub workflow validates the complete core. The Homebrew workflow
validates the server-only formula on macOS before it submits a Core pull request.

## Homebrew releases

`Formula/indiserver.rb` is a server-only formula. The `Homebrew` workflow
validates it on relevant pull requests, then opens or updates a pull request
against `Homebrew/homebrew-core` when a GitHub release is published, or when
started manually. Homebrew maintainers must review and merge that pull request
before `brew install indiserver` is available. Configure the source repository
with:

- Repository variable `HOMEBREW_CORE_FORK`: a maintained fork of
  `Homebrew/homebrew-core`, such as `indilib/homebrew-core`.
- Repository secret `HOMEBREW_CORE_TOKEN`: a fine-grained token or GitHub App
  token with access to push to that fork and create pull requests against
  `Homebrew/homebrew-core`.

For a manual run, provide the release tag. Select **Open or update a Homebrew
Core PR** only when the fork and token are configured; otherwise the workflow
performs validation only. A submission changes only `Formula/i/indiserver.rb`
on its release branch; reruns update the same pull request instead of opening
duplicates.
