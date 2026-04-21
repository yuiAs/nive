# MSIX Packaging

This document explains how nive is packaged as an MSIX for distribution
through the Microsoft Store, and why the package declares the
`runFullTrust` restricted capability.

## Why nive declares the `runFullTrust` capability

nive is a lightweight native image viewer for Windows, distributed
as an MSIX package for installation through the Microsoft Store.
The package declares the restricted capability `runFullTrust`
because the application is a classic Win32 desktop application
whose core functionality cannot be implemented within the UWP
application sandbox.

### Technical reasons the capability is required

1. **Native rendering stack.** nive renders images using Direct2D
   and DirectWrite directly against a Win32 HWND with custom
   message-loop handling, DPI-aware window subclassing, and
   non-client area painting. These are desktop Win32 patterns
   outside the UWP rendering model.

2. **Dynamic codec plugin loading.** Additional image formats are
   supported by loadable plugins (for example, AVIF via
   `plugins/nive_avif.dll`). The host resolves and loads plugin
   DLLs at runtime through `LoadLibrary`, which is not permitted
   inside the sandboxed UWP process model.

3. **Third-party native dependencies.** nive links against native
   libraries built for the Win32 target (libavif, libaom, bit7z,
   zstd, spdlog, tomlplusplus). These libraries use POSIX/Win32
   file APIs and process-wide state that are incompatible with the
   UWP API surface.

4. **Arbitrary file-system read access.** As a general-purpose
   image viewer, nive must be able to open image files from any
   location the user chooses, including paths outside of the
   Pictures or Documents known folders. The sandboxed
   `broadFileSystemAccess` model is insufficient because nive is
   launched both interactively and as a file-type handler from
   Explorer, where the delivered path may lie on any drive.

### How the capability is actually used

- Reading the image file the user explicitly opens (via file
  picker, drag-and-drop, or shell "Open with" activation).
- Loading image-codec plugin DLLs from the application's own
  `plugins/` directory.
- Writing diagnostic log output to the per-user application data
  directory via spdlog.

### What the capability is **not** used for

nive performs no elevated or system-wide actions. Specifically, it
does **not**:

- collect, store, or transmit any personal information;
- send telemetry, analytics, or crash reports to any server;
- open outbound network connections of any kind;
- read, write, or enumerate data belonging to other applications;
- modify system-wide state, registry, or protected directories;
- run background tasks or persist processes beyond the user's
  viewing session.

All processing occurs locally on the user's device, scoped to the
image files the user directly opens.

## Build pipeline

The MSIX package is produced by `scripts/create-msix.ps1`, invoked
automatically by `scripts/build-release.ps1` when the `-CreateMsix`
switch is passed.

```powershell
# Produce both ZIP and MSIX
./scripts/build-release.ps1 -CreateMsix

# Produce MSIX only from an existing staging directory
./scripts/create-msix.ps1 -StagingDir ./dist/nive-v0.4.0
```

The manifest template lives at `packaging/msix/AppxManifest.xml`
and uses `{{PLACEHOLDER}}` tokens that are substituted at pack time
with values taken from `CMakeLists.txt` and script parameters.

Visual assets (tile logos, splash screen, store logo) are generated
at pack time from `resources/icons/nive.png` via `System.Drawing`.

## Signing

The produced `.msix` is intentionally left **unsigned**. The
Microsoft Store re-signs submissions with its own certificate as
part of the submission pipeline, so local signing is not required
for store distribution. To test-install the package locally, sign
it with a trusted certificate using `signtool.exe` as a separate
step.

## Store submission notes

- **Publisher identity** and **Package Identity Name** are assigned
  by Partner Center when the app is reserved. Current values are
  hard-coded as defaults in `scripts/create-msix.ps1`; override
  them via parameters if the Store identity changes.
- **Privacy policy URL** is mandatory for Win32/Desktop Bridge
  products per Microsoft Store Policy 10.5.1, regardless of whether
  any personal information is actually collected. This is a store
  policy requirement, not a manifest-level constraint.
