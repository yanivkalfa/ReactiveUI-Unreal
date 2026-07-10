# Security policy

## Reporting a vulnerability

Please **do not open a public issue** for security-sensitive reports (e.g. anything exploitable
through `.uetkx` parsing/interpretation, the expression VM sandbox, or the build-integration
scripts). Instead use GitHub's private vulnerability reporting on this repository
(Security → "Report a vulnerability"), or contact the maintainer via the
[Discord server](https://discord.gg/Knedqu4Wyv) (DM the owner).

You can expect an acknowledgment within a few days. Fixes ship as a patch release with a
changelog entry; credit is given unless you ask otherwise.

## Scope notes

- The dev-mode expression VM is deliberately restricted (side-effect-free, whitelisted calls,
  iteration caps) and is compiled out of Shipping builds entirely — reports that it can be made
  to run arbitrary code in a SHIPPED game are top severity.
- The plugin ships no network code, no telemetry, and no external binaries.
