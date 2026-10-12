# Security

Status: normative

How RZI receives, triages, and reports vulnerabilities, and what a product
built on RZI must do to meet the EU Cyber Resilience Act (CRA) obligations
that apply since 11 September 2026.

See also: [sbom.md](./sbom.md), [ci.md](./ci.md),
[west-patch.md](./west-patch.md).

## Reporting a vulnerability

Report privately through GitHub: repository **Security** tab, **Report a
vulnerability**. Do not open a public issue. Include the RZI version
(`RZI_VERSION_STRING` or the release tag), the board, and a reproduction.

## CRA reporting clock

For an actively exploited vulnerability, the manufacturer of the product
reports to the CSIRT designated by its member state, through the ENISA single
reporting platform. The clock starts when the manufacturer becomes aware:

| Deadline | Report |
|---|---|
| 24 hours | Early warning |
| 72 hours | Notification with impact and mitigations |
| 14 days after a fix is available | Final report |

RZI maintainers acknowledge a private report within one working day and
share a triage verdict within 72 hours, so a product team can meet its own
deadlines. Rehearse the 24 h / 72 h path before it is needed.

## Triage

A CVE is assessed against a specific firmware build, not against the
workspace:

1. **Identify.** Match the CPE / PURL in the release SBOMs against NVD and
   OSV.dev. RZI is `cpe:2.3:a:rakwireless:rzi:<version>`; Zephyr modules
   carry their own references in `modules-deps.jsonld`.
2. **Scope.** Check in `zephyr.jsonld` and `build.jsonld` whether the
   affected files were compiled into the image. A module that contributed
   no files is not in the firmware. The SPDX 3 visualizer's impact view
   (<https://kartben.github.io/spdx3_viz>) shows this per element.
3. **Assess.** Record the verdict as a VEX statement (below).
4. **Act.** Patch, cut a release, and inform users; report under the CRA
   when the vulnerability is actively exploited.

## VEX

Vulnerability status is not part of the SBOM. Verdicts are published as
OpenVEX documents next to the release SBOMs, one statement per CVE and
product, using the PURL from the SBOM as the product identifier:

| Status | Use |
|---|---|
| `not_affected` | With a justification, e.g. `vulnerable_code_not_present` when the affected files are not compiled in |
| `affected` | With an action statement: the fixed release or a mitigation |
| `fixed` | The release that contains the fix |
| `under_investigation` | Until the assessment is complete |

## Upstream alerts

Watch the sources of every component in the SBOM:

- Zephyr: security advisories at
  <https://github.com/zephyrproject-rtos/zephyr/security/advisories> and the
  Zephyr vulnerability page in the project documentation. Register as a
  product maker with the Zephyr security team for embargoed notifications.
- Semtech USP and usp_zephyr: GitHub security advisories and releases of
  `Lora-net/usp` and `Lora-net/usp_zephyr`.
- MCUboot and the SoC HALs selected by the product workspace.

## Supported base

Products should ship on a Zephyr release that receives security fixes,
preferably an LTS release. `west.yml` currently pins a Zephyr release
candidate for development; a release of RZI meant for products must move to
a supported Zephyr release and record it in the module SBOM.
