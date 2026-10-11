# Security Policy

## Reporting a vulnerability

Please do not open a public issue or pull request for a security vulnerability in libzedmd.

Report it by email to **git@kalki.de** instead. Include what you found, the version or commit it applies to,
and the smallest input or steps that reproduce it. A proposed patch is welcome but not required.

You will get a reply as soon as the report has been read. This is a volunteer-maintained project, so there is
no guaranteed response time, but security reports are handled before other work. Please give the fix a
reasonable time to be released before disclosing the problem publicly.

## Scope

In scope is how libzedmd handles data it does not control: bytes received from a device over USB serial, and
responses from a network peer in WiFi mode. Bugs in the bundled third-party code (cargs, libserialport, sockpp,
miniz) should go to those projects, and bugs in the display firmware to the ZeDMD repository.

Only the latest release and the default branch are supported; fixes are not backported to older versions.
