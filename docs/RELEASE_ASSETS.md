# Automated C++ source release archives

GitHub automatically creates basic source ZIP/TAR archives for Git tags. This
repository also uses `.github/workflows/release-assets.yml` to validate a
C++17 release build, run CTest and attach CPack source archives and SHA-256
checksums.

On every push to `main` and pull request, the workflow builds, tests and
validates the source distributions. Once you publish a GitHub release it
uploads the verified archives as release assets automatically.

For a new release:

1. Update the CMake `project(... VERSION X.Y.Z ...)` source version.
2. Ensure CI passes, create matching tag `vX.Y.Z` and publish the release.
3. GitHub builds, tests, packages and uploads archives automatically.

This upload does not require a package-registry account or recurring manual
work. The release workflow refuses to overwrite existing assets or bypass
tests. These are **source distributions**, not a ConanCenter/vcpkg port,
prebuilt platform binaries, or a separate package registry.
