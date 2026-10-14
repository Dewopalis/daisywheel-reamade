#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
[Setup]
AppId={{6CBE5A79-EE66-44B9-8670-4266248F8EBC}
AppName=Daisywheel REAmade
AppVersion={#AppVersion}
AppPublisher=Daisywheel REAmade contributors
AppPublisherURL=https://github.com/Dewopalis/daisywheel-reamade
DefaultDirName={localappdata}\Programs\Daisywheel REAmade
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=daisywheel-reamade-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\bin\daisywheel.exe
CloseApplications=yes
[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{autoprograms}\Daisywheel REAmade"; Filename: "{app}\bin\daisywheel.exe"; Parameters: "--windows-input"
Name: "{autodesktop}\Daisywheel REAmade"; Filename: "{app}\bin\daisywheel.exe"; Parameters: "--windows-input"; Tasks: desktopicon
[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked
[Run]
Filename: "{app}\bin\daisywheel.exe"; Parameters: "--windows-input"; Description: "Launch Daisywheel REAmade"; Flags: nowait postinstall skipifsilent
[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "DaisywheelREAmade"; Flags: uninsdeletevalue
