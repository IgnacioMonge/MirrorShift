# Third-party notices

Mirror Shift is distributed under GNU General Public License version 2. The
license text is in `LICENSE`. The corresponding application source is at
<https://github.com/IgnacioMonge/MirrorShift-dev>; a binary release must retain
or attach the source archive for the same release revision.

## Qt 6.11.0 in the Windows x86_64 package

The Windows package contains these QtBase binaries:

| Distributed file | Qt SBOM package |
|---|---|
| `Qt6Core.dll` | Core |
| `Qt6Gui.dll` | Gui |
| `Qt6Network.dll` | Network |
| `Qt6Widgets.dll` | Widgets |
| `imageformats/qjpeg.dll` | QJpegPlugin |
| `platforms/qwindows.dll` | QWindowsIntegrationPlugin |

Qt's installed 6.11.0 SPDX document declares each of these packages under
`LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`.
This Mirror Shift distribution selects the existing `GPL-2.0-only` option; it
does not relicense Mirror Shift. The applicable Qt licence text is
`licenses/Qt-6.11.0/LICENSES/GPL-2.0-only.txt`.

The exact Qt installation inventory, dependency relationships used to derive
this package subset, copyright notices, versions, source locations and licence
expressions are preserved in
`licenses/Qt-6.11.0/qtbase-6.11.0.spdx` (SHA-256
`1f219fd7f1aeb4dd0df7dd06b9377af8d2383fc46946623194bc5470022eb285`).
The licence texts referenced by the dependency closure of the six SBOM
packages above are in `licenses/Qt-6.11.0/LICENSES/`.

The corresponding QtBase source is the official archive
<https://download.qt.io/archive/qt/6.11/6.11.0/submodules/qtbase-everywhere-src-6.11.0.zip>
(SHA-256 `590d5ae246c85fa14d6458a36ff75a11236acfe8987c2475090aab1770acbdf8`),
which records QtBase commit `8ba7ea4b77a4b8f1948760221e264917ddc9e1c8`.

The deployed JPEG plugin includes libjpeg-turbo under the IJG and BSD
3-Clause licences. This software is based in part on the work of the
Independent JPEG Group. Its notices and terms are preserved by the SPDX file
and `licenses/Qt-6.11.0/LICENSES/IJG.txt` plus `BSD-3-Clause.txt`.

## Microsoft Visual C++ runtime in the Windows x86_64 package

The package copies the unmodified DLLs from Visual Studio's
`VC/Redist/MSVC/<version>/x64/Microsoft.VC143.CRT` directory: `concrt140.dll`,
`msvcp140.dll`, `msvcp140_1.dll`, `msvcp140_2.dll`,
`msvcp140_atomic_wait.dll`, `msvcp140_codecvt_ids.dll`, `vccorlib140.dll`,
`vcruntime140.dll`, `vcruntime140_1.dll`, and
`vcruntime140_threads.dll`. Microsoft permits licensed Visual Studio users to
redistribute unmodified files from `VC/Redist`, subject to the Visual Studio
licence terms:
<https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution#visual-c-runtime-files>.

`icuuc.dll` is a Windows system library and is not distributed. Microsoft
documents it as part of Windows since Windows 10 version 1703:
<https://learn.microsoft.com/en-us/windows/win32/intl/international-components-for-unicode--icu->.
Qt 6.11 supports Windows 10 version 1809 or later and Windows 11.

macOS and Linux packages use their target Qt builds. Their final archives must
carry and be checked against the matching target-build SBOM before release.
