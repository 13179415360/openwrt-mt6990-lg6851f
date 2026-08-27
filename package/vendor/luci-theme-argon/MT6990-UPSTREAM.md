# MT6990 import notes

- Upstream: https://github.com/jerrykuku/luci-theme-argon
- Imported tag: `v2.4.6`
- Imported commit: `136eb5d42f30554e89cc737fd90f503909810660`
- Local package version was corrected to `2.4.6-r20260731` because the tag
  still carried the previous release metadata in its Makefile.
- Bootstrap remains enabled as a recovery theme. This theme is also enabled in
  the LG6851F/MT6990 `.config` and includes responsive guards for the local 5G
  modem, PWM fan and OpenClash LuCI pages.
- The install default only registers Argon and does not force it as the active
  theme, preserving Bootstrap as a known-good first-boot and recovery path.
