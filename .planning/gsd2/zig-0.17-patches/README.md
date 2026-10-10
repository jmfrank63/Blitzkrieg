# Zig 0.17 package pins

No patches are applied to any fetched package. This folder keeps the convention (D059): a future
package fix goes here as `<package>.patch` with the upstream URL and commit.

- sdl comes from upstream castholm/SDL v0.6.0+3.4.18 at dbcb19aa (SDL 3.4.18). The maintainer chose it
  because it configures on Zig 0.17 unpatched. It replaced `--sysroot` with the package options
  `system_include_path`, `system_framework_path` and `library_path`; the root build forwards
  `-Dsystem_include_path`, `-Dsystem_framework_path` and `-Dlibrary_path` to it.
- dxc comes from jmfrank63/dxc-build branch zig-0.17 at bdf91da3 (upstream PR Gota7/dxc-build#2, with the
  reviewed fix that keeps `zig build version` printing the version), until upstream merges it.
