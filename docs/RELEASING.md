# Releasing DUNGEON CRAWLER and DUNGEON CRAWLER FX (macOS signing and notarisation)

DUNGEON CRAWLER (the instrument) and DUNGEON CRAWLER FX (the effect) are signed with the
Developer ID of **LJP Records Inc.** (team ID **YYZP4W7G7D**, brand Windward Audio), notarised by
Apple and shipped as a signed installer package, **DUNGEON CRAWLER.pkg**. This follows the
approach of the LIMINAL repository (VACANCY). All of it happens in CI
(`.github/workflows/build.yml`, job `macos`, on `macos-15`).

1.0.0 is frozen: bug fixes only ([ROADMAP.md](ROADMAP.md)). The version lives in one place,
`project(... VERSION 1.0.0)` in `CMakeLists.txt`; hosts, bundles and the installer read it from there.

## What CI does

| Step | Script | When |
| --- | --- | --- |
| Decide whether the run signs (event, fork, all seven secrets present) | inline in the workflow | always |
| Temporary keychain with both certificates and Apple's Developer ID G2 intermediate (checked against Apple Root CA); API key file | `scripts/ci-signing-setup.sh` | signing runs |
| Sign `DungeonCrawler.component`, `DungeonCrawler.vst3`, `DungeonCrawlerFX.component`, `DungeonCrawlerFX.vst3` inside out (nested binaries first, then the bundle): Developer ID Application, hardened runtime, secure timestamp, no entitlements | `scripts/sign-mac.sh` | signing runs |
| Ad-hoc signing of whatever is not Developer ID signed, developer zip `DungeonCrawler-macOS-Universal.zip` | `scripts/package-mac.sh` | always |
| Notarise the zip; staple the tickets to the four bundles; re-package the zip with the stapled bundles | `scripts/notarize-mac.sh` | signing runs |
| Build `DUNGEON CRAWLER.pkg` with `pkgbuild` + `productbuild`, signed with Developer ID Installer | `scripts/build-pkg.sh` | always (unsigned without signing) |
| Notarise `DUNGEON CRAWLER.pkg`, staple and validate the ticket | `scripts/notarize-mac.sh --staple` | signing runs |
| `codesign --verify --deep --strict`, `spctl --assess` for the bundles (`--type open`) and the installer (`--type install`): each must say `accepted` and `source=Notarized Developer ID` | `scripts/ci-verify-signing.sh` | signing runs |
| Zip path: quarantined download, `install-mac.sh --ci`, auval and AU harnesses (with a session saved before 1.0) | `scripts/ci-user-install-test.sh` | always |
| Package path: Gatekeeper on the quarantined `.pkg`, old copies planted, `sudo installer -pkg`, receipts, bundle IDs and version 1.0.0, old copies removed, registry (each plug-in once, as "Windward Audio: ..."), codesign / Gatekeeper on the installed copies, auval and AU harnesses arm64 + x86_64 on the installed copies, the preset migration | `scripts/ci-pkg-install-test.sh` | always (Gatekeeper parts only when notarised) |
| Customer download `DUNGEON CRAWLER-1.0-macOS` (only `DUNGEON CRAWLER Installer.pkg` and the Read Me) | `scripts/package-mac.sh --customer` | signing runs |
| Delete the keychain and the key file, restore the keychain search list | `scripts/ci-signing-cleanup.sh` | always, last step |

Artifacts:

| Artifact | Contents | Kept |
| --- | --- | --- |
| `DUNGEON CRAWLER-1.0-macOS` | the customer download, to the [installer standard](INSTALLER_STANDARD.md): only `DUNGEON CRAWLER Installer.pkg` (signed, notarised, stapled) and `DUNGEON CRAWLER - Read Me.pdf`; signed runs only | 14 days |
| `DUNGEON-CRAWLER-dev-macOS` | the developer zip: the four bundles, `install-mac.sh`, `uninstall-mac.sh`, `diagnose-mac.sh`, `INSTALL.txt`; every green run; for testing only | 14 days |
| `DUNGEON-CRAWLER-macOS-diagnostics` | install, auval and harness logs; notarisation logs under `notary/` | 5 days |
| `DUNGEON-CRAWLER-linux-test-logs` | the test suite's logs and the listening renders | 5 days |

### When it signs

Only **pushes to `main`** and **manual runs** (Actions > Native build > Run workflow, which only
people with write access can start) of this repository, never pull requests and never forks.
Every other run, and any run where one of the secrets is missing, is the ad-hoc build: a notice
titled "Ad-hoc build" says why, the bundles are signed ad hoc, `DUNGEON CRAWLER.pkg` is built
unsigned so the installer is still tested, and no customer artifact is produced. CI never fails
because the secrets are absent.

### Releases

Tag pushes (`v*`) are not signing runs: they build and test only. To publish a signed release:

```bash
git tag -a v1.0.0 -m "DUNGEON CRAWLER 1.0.0" && git push origin v1.0.0
```

```bash
gh workflow run build.yml --ref v1.0.0
```

The manual run on the tag signs and notarises, and the `release` job (manual runs on
`refs/tags/v*` only) attaches `DUNGEON CRAWLER-1.0-macOS.zip` (`DUNGEON CRAWLER Installer.pkg` and the Read Me) to the
GitHub Release of that tag, creating the release if needed.

## Secrets (Settings > Secrets and variables > Actions)

| Secret | Contents |
| --- | --- |
| `MACOS_APP_CERT_P12` | base64 of the **Developer ID Application** certificate and its private key (.p12) |
| `MACOS_APP_CERT_PASSWORD` | password of that .p12 |
| `MACOS_INSTALLER_CERT_P12` | base64 of the **Developer ID Installer** certificate and its private key (.p12) |
| `MACOS_INSTALLER_CERT_PASSWORD` | password of that .p12 |
| `ASC_KEY_ID` | App Store Connect API key ID (10 characters) |
| `ASC_ISSUER_ID` | App Store Connect issuer ID (a UUID) |
| `ASC_KEY_P8` | base64 of the API key file `AuthKey_<KEYID>.p8` |

Secrets are passed only to the steps that use them, as environment variables, and never echoed;
the .p12 files are deleted right after import, the keychain password is random and masked. The
team ID is not a secret and is set in the workflow (`APPLE_TEAM_ID`). The setup script finds the
identities by team ID and fails clearly if a .p12 holds a different team's certificate. (The old
`MACOS_CERTIFICATE` / `NOTARY_*` secrets of the untested pre-1.0 path are no longer used.)

Encode files for a secret without line breaks:

```bash
base64 -i DeveloperIDApplication.p12 | pbcopy
```

## Entitlements

None. A plug-in is a library loaded into its host's process (GarageBand, Logic Pro, another
music app, or Apple's AUHostingService for out-of-process Audio Units): the host's entitlements
apply and a library's own are ignored. The plug-ins need no JIT, no unsigned executable memory,
no library-validation exception and no microphone, network or file entitlement of their own.
`scripts/sign-mac.sh` fails if a bundle ends up carrying entitlements. (The standalone apps,
built for development and not shipped, stay ad-hoc signed.)

## Identity: what changed in 1.0 and what must never change

| | Value | Changeable? |
| --- | --- | --- |
| Name hosts show | "Windward Audio: DUNGEON CRAWLER", "Windward Audio: DUNGEON CRAWLER FX" (`COMPANY_NAME`) | yes |
| Bundle IDs | `com.windwardaudio.dungeoncrawler`, `com.windwardaudio.dungeoncrawlerfx` (before 1.0: `audio.veil.dungeoncrawler.instrument`, `audio.veil.dungeoncrawler`) | no longer |
| AU type / subtype / manufacturer code | `aumu Dgci Veil`, `aufx Dgcr Veil` | **never**: projects find the plug-ins by these codes |
| VST3 class IDs | derived by JUCE from the same codes | **never** |
| Parameter IDs, state format | `Source/Parameters.h`, `getStateInformation` | **never** remove or rename; append only |

`Tests/fixtures/session-pre-1.0-*.xml` are states saved before 1.0: the Linux suite (group
`release`) and both AU harnesses (`--old-session`, as a host's ClassInfo with the old codes)
check they restore every parameter unchanged.

## The installer package

Pages, wording, background, licence and Read Me follow the Windward Audio
[installer standard](INSTALLER_STANDARD.md). `DUNGEON CRAWLER.pkg` (shipped as `DUNGEON CRAWLER Installer.pkg`) installs for all users:

- `/Library/Audio/Plug-Ins/Components/DungeonCrawler.component` and `DungeonCrawlerFX.component` (choice "Audio Units")
- `/Library/Audio/Plug-Ins/VST3/DungeonCrawler.vst3` and `DungeonCrawlerFX.vst3` (choice "VST3")

The sounds ship inside the bundles (`Contents/Resources/Samples`), with the painted art; the
factory presets of both plug-ins are compiled into their binaries, so they install with the
bundles. The pages (Welcome, Read Me, License, Summary) are rendered from `installer/templates` by
`installer/render.py`.

Before installing, the preinstall scripts (`packaging/pkg/remove-copies.sh`) silently remove
(logged only to `/var/log/install.log`, lines starting with `DUNGEON CRAWLER installer:`),
system-wide and in every user's `~/Library`:

- copies installed before 1.0, by their old bundle IDs (`audio.veil.dungeoncrawler*`), whatever
  the bundle is called, in both formats
- per-user copies of DungeonCrawler / DungeonCrawlerFX (e.g. from the developer zip and
  `install-mac.sh`) and renamed copies with the new bundle IDs, which hosts would list twice

Nothing else is touched: other plug-ins, and the user's presets, stay. The bundles are marked
non-relocatable, so the installer always writes to `/Library`. The AU postinstall restarts
`AudioComponentRegistrar`.

## Users' files and the migration

| What | Since 1.0 | Before 1.0 (copied from, kept) |
| --- | --- | --- |
| Presets saved in the plug-in | `~/Library/Windward Audio/DUNGEON CRAWLER[ FX]/Presets` | `~/Library/Application Support/DungeonCrawler/Presets` (shared by both) |
| Drop-in scene art | `~/Library/Windward Audio/DUNGEON CRAWLER[ FX]/scenes` | `~/Library/Application Support/DungeonCrawler/scenes` |
| Host presets (.aupreset) | `~/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER[ FX]` | `~/Library/Audio/Presets/<old company>/...` (sorted by the AU subtype saved in each file) |

The first time each plug-in is loaded, `Source/UserData.cpp` copies (never moves) the old files,
never overwrites a file already in the new place, and writes `.migrated-to-1.0` in the new folder
so it happens once. Sandboxed hosts that cannot reach the old folders simply skip it; it runs
again the next time the plug-in loads in a host that can.

## Uninstall

```bash
sudo rm -rf /Library/Audio/Plug-Ins/Components/DungeonCrawler.component /Library/Audio/Plug-Ins/Components/DungeonCrawlerFX.component /Library/Audio/Plug-Ins/VST3/DungeonCrawler.vst3 /Library/Audio/Plug-Ins/VST3/DungeonCrawlerFX.vst3
```

```bash
sudo pkgutil --forget com.windwardaudio.pkg.dungeoncrawler.au; sudo pkgutil --forget com.windwardaudio.pkg.dungeoncrawler.vst3; sudo pkgutil --forget com.windwardaudio.pkg.dungeoncrawler.prepare
```

## The customer Read Me

`DUNGEON CRAWLER - Read Me.pdf` is rendered by `installer/render.py` from the standard's template
`installer/templates/readme-pdf.txt.in` and `installer/product.json` on every package build, next
to the pkg ([installer standard](INSTALLER_STANDARD.md)). CI checks that the installer's pages
mention no old name or script.

## Renewing the certificates

The setup step prints each certificate's expiry date on every signing run and warns when fewer
than 60 days remain. Code signed and notarised before expiry keeps working: the secure
timestamp proves when it was signed. Only new builds need the new certificate.

1. On a Mac, in **Keychain Access > Certificate Assistant > Request a Certificate From a
   Certificate Authority**, create a certificate signing request (saved to disk).
2. As the Account Holder of LJP Records Inc., go to
   [developer.apple.com/account/resources/certificates](https://developer.apple.com/account/resources/certificates),
   click **+**, choose **Developer ID Application** (G2 Sub-CA), upload the request and download
   the certificate; repeat for **Developer ID Installer**. (Revoke a certificate only if its key
   was compromised: revoking invalidates what it signed.)
3. Double-click each downloaded `.cer` so it joins its private key in the login keychain. In
   **Keychain Access > My Certificates**, export each identity (with its key) as .p12 with a
   strong password.
4. Replace `MACOS_APP_CERT_P12` / `MACOS_APP_CERT_PASSWORD` and `MACOS_INSTALLER_CERT_P12` /
   `MACOS_INSTALLER_CERT_PASSWORD` (base64 as above).
5. Start **Run workflow** on `main` and check that the run is green and the "Import the
   Developer ID certificates" step prints the new expiry date.

### Rotating the App Store Connect API key

1. [appstoreconnect.apple.com/access/integrations/api](https://appstoreconnect.apple.com/access/integrations/api)
   > Team Keys > **+**, access **Developer** (enough for notarisation). Download
   `AuthKey_<KEYID>.p8` (only possible once).
2. Replace `ASC_KEY_ID`, `ASC_ISSUER_ID` and `ASC_KEY_P8` (`base64 -i AuthKey_<KEYID>.p8`).
3. Run the workflow, then revoke the old key in App Store Connect.

## Troubleshooting

- **"could not import the ... certificate (wrong password?)"**: the password secret does not
  match the .p12, or the secret is not base64 of the .p12. (A .p12 exported by OpenSSL 3 is
  re-packed automatically.)
- **"no valid 'Developer ID Application' identity of team YYZP4W7G7D"**: the .p12 holds another
  certificate, lacks its private key, or the certificate expired or was revoked.
- **Notarisation "Invalid"**: the step prints Apple's log; each issue names the file and the
  reason. The log is also in the diagnostics artifact (`notary/*.notary-log.json`).
- **Notarisation "unauthorized"**: `ASC_KEY_ID`, `ASC_ISSUER_ID` and `ASC_KEY_P8` do not belong
  together, or the key was revoked.
- Check a downloaded package on any Mac:

  ```bash
  spctl --assess --type install -vvv "DUNGEON CRAWLER.pkg"
  ```

  ```bash
  pkgutil --check-signature "DUNGEON CRAWLER.pkg"
  ```
