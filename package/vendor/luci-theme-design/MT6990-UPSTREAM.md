# MT6990 import notes

- Upstream: https://github.com/0x676e67/luci-theme-design
- Imported commit: `5cea733806ccaf17fd63790d171bb2b65f29f8bd`
- The upstream theme uses legacy Lua templates. The local package therefore
  declares `luci-compat`; its transitive `luci-lua-runtime` dependency is
  verified by the APK build.
- The upstream `5.8.0-20240106` version was normalized to the APK-compatible
  `5.8.0-r20240106` form.
- Bootstrap remains enabled as a recovery theme. This theme is also enabled in
  the LG6851F/MT6990 `.config` and includes responsive guards for the local 5G
  modem, PWM fan and OpenClash LuCI pages.
- The install default only registers Design and does not force it as the active
  theme, preserving Bootstrap as a known-good first-boot and recovery path.
