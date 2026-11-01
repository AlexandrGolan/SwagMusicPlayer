; Inno Setup script for Swag Music Player (SMP).
; Build with installer\build.ps1, which stages the release binary first.

#define AppName "Swag Music Player"
#define AppShortName "SMP"
#define AppVersion "2.2.0"
#define AppPublisher "AlexandrGolan"
#define AppURL "https://github.com/AlexandrGolan/SwagMusicPlayer"
#define AppExeName "smp.exe"

[Setup]
AppId={{7C1B4E52-2F6A-4B0E-9A3D-5E8C1D4A7B21}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
OutputDir=..\dist
OutputBaseFilename=SMP-Setup-{#AppVersion}
SetupIconFile=smp.ico
UninstallDisplayIcon={app}\smp.ico
UninstallDisplayName={#AppName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
ShowLanguageDialog=auto

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
Source: "stage\smp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "smp.cmd"; DestDir: "{app}"; Flags: ignoreversion
Source: "smp.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "stage\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "stage\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\smp.cmd"; WorkingDir: "{%USERPROFILE}"; IconFilename: "{app}\smp.ico"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\smp.cmd"; WorkingDir: "{%USERPROFILE}"; IconFilename: "{app}\smp.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\smp.cmd"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; WorkingDir: "{%USERPROFILE}"; Flags: nowait postinstall skipifsilent

[Code]
{ The player asks for the music folder itself, so the wizard needs no code. }
