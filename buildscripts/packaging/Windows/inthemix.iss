; InTheMix by Anonymous Tracks -- Windows installer (Inno Setup 6).
;
; Packs a staging folder made by make_installer.ps1 from the CMake install
; prefix (bin\InTheMix.exe, Qt, QML, locale), plus FFmpeg (LGPL build) in
; bin\ffmpeg and every licence in licenses\.
;
;   iscc /DStageDir=D:\InTheMix\stage /DAppVersion=1.0.0 /DArtDir=... inthemix.iss
;
; Unsigned builds meet SmartScreen ("Windows protected your PC" -> More info
; -> Run anyway).

#define AppName "InTheMix"
#define AppExe "InTheMix.exe"
#define Publisher "Anonymous Tracks"
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef AppVersionLabel
  #define AppVersionLabel "1.0"
#endif
#ifndef StageDir
  #error Pass the staging folder: iscc /DStageDir=<folder> inthemix.iss
#endif
#define RepoRoot AddBackslash(SourcePath) + "..\..\.."
#ifndef OutDir
  #define OutDir RepoRoot + "\build.release"
#endif

[Setup]
; Fixed for every InTheMix release (not GlideMix's): Windows finds the
; installed copy with it for updates and uninstall.
AppId={{AF22AE6C-E763-48A6-AFB9-1ACC7E51BA8E}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} Beta v{#AppVersionLabel}
AppPublisher={#Publisher}
AppPublisherURL=https://anonymoustracks.com
AppSupportURL=https://anonymoustracks.com
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#Publisher}
VersionInfoDescription={#AppName} by {#Publisher} Setup
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableWelcomePage=no
; Per-user by default (no admin prompt); the dialog offers all users.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutDir}
OutputBaseFilename={#AppName}-Beta-v{#AppVersionLabel}-x64-setup
SetupIconFile={#RepoRoot}\share\icons\AppIcon\GlideMix.ico
UninstallDisplayIcon={app}\bin\{#AppExe}
UninstallDisplayName={#AppName} by {#Publisher}
; GPL: no licence to accept -- the texts are installed in licenses\.
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
#ifdef ArtDir
WizardImageFile={#ArtDir}\wizard-side.bmp
WizardSmallImageFile={#ArtDir}\wizard-small.bmp
#endif
CloseApplications=yes
; The language picker opens first; English is the default.
ShowLanguageDialog=yes
UsePreviousLanguage=no
LanguageDetectionMethod=none

[Languages]
; The installer's language is also the app's (see [INI] below). English first = default.
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "portuguesebr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "swedish"; MessagesFile: "compiler:Languages\Swedish.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\bin\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\bin\{#AppExe}"; Tasks: desktopicon

[INI]
; The app's language (%APPDATA%\Glidemix\Glidemix.ini, [languages] language).
; Not on a silent update: the user's own choice in Preferences stays.
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "en@placeholder"; Languages: english; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "it"; Languages: italian; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "es"; Languages: spanish; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "fr"; Languages: french; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "de"; Languages: german; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "pt_BR"; Languages: portuguesebr; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "pt_PT"; Languages: portuguese; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "nl"; Languages: dutch; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "pl"; Languages: polish; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "ru"; Languages: russian; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "uk"; Languages: ukrainian; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "tr"; Languages: turkish; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "cs"; Languages: czech; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "sv"; Languages: swedish; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "ja"; Languages: japanese; Check: not WizardSilent
Filename: "{userappdata}\Glidemix\Glidemix.ini"; Section: "languages"; Key: "language"; String: "ko"; Languages: korean; Check: not WizardSilent

[Run]
Filename: "{app}\bin\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
; An update run silently reopens the app after it.
Filename: "{app}\bin\{#AppExe}"; Flags: nowait; Check: WizardSilent

; User data (%APPDATA%\Glidemix, %LOCALAPPDATA%\Glidemix: library, settings,
; mixes) is kept on uninstall.
