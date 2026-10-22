; Inno Setup script for Ton of Able (Windows 10/11, 64 bit).
; Built by .github/workflows/release-windows.yml:
;   ISCC.exe /DAppVersion=0.2.0 /DSourceDir="...\dist\Ton of Able" /DOutputName=Ton-of-Able-v0.2.0-setup TonOfAble.iss
; SourceDir is the same folder that goes into the portable ZIP.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\Ton of Able"
#endif
#ifndef OutputName
  #define OutputName "Ton-of-Able-setup"
#endif
#define AppExe "Ton of Able.exe"

[Setup]
; Never change the AppId: updates and the uninstaller find earlier installs by it.
AppId={{394C2EE4-C0B7-4F22-9AED-CD3765B71064}
AppName=Ton of Able
AppVersion={#AppVersion}
AppVerName=Ton of Able {#AppVersion}
AppPublisher=Ton of Able
AppPublisherURL=https://github.com/nikh93/Ton-of-Able
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\Ton of Able
DefaultGroupName=Ton of Able
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Program Files for everyone by default; the dialog also allows a per-user install without admin rights.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\..\installer-out
OutputBaseFilename={#OutputName}
SetupIconFile=..\icon\app.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName=Ton of Able
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes
InfoBeforeFile={#SourceDir}\LIESMICH.txt

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
de.AssocTask=Projektdateien (.tonproj) mit Ton of Able öffnen
en.AssocTask=Open project files (.tonproj) with Ton of Able
de.AssocGroup=Dateizuordnung:
en.AssocGroup=File association:
de.PresetTask=Werks-Presets für Valhalla Supermassive installieren
en.PresetTask=Install the factory presets for Valhalla Supermassive
de.PluginGroup=Plugins:
en.PluginGroup=Plugins:
de.ProjectType=Ton of Able Projekt
en.ProjectType=Ton of Able project
de.ReadMe=Ton of Able – Liesmich
en.ReadMe=Ton of Able – Read me

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "tonproj"; Description: "{cm:AssocTask}"; GroupDescription: "{cm:AssocGroup}"
Name: "presets"; Description: "{cm:PresetTask}"; GroupDescription: "{cm:PluginGroup}"

[Files]
; The app finds its resources, FFmpeg and the bundled VST3 plugins next to the exe.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "Supermassive-Presets"; Flags: recursesubdirs createallsubdirs ignoreversion
; Supermassive looks for its factory presets here; they stay when Ton of Able is uninstalled.
Source: "{#SourceDir}\Supermassive-Presets\Factory\*"; DestDir: "{commonappdata}\Valhalla DSP, LLC\ValhallaSupermassive\Presets\Factory"; \
    Tasks: presets; Flags: recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\Ton of Able"; Filename: "{app}\{#AppExe}"
Name: "{autoprograms}\{cm:ReadMe}"; Filename: "{app}\LIESMICH.txt"
Name: "{autodesktop}\Ton of Able"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.tonproj"; ValueType: string; ValueName: ""; ValueData: "TonOfAble.Project"; Flags: uninsdeletevalue; Tasks: tonproj
Root: HKA; Subkey: "Software\Classes\TonOfAble.Project"; ValueType: string; ValueName: ""; ValueData: "{cm:ProjectType}"; Flags: uninsdeletekey; Tasks: tonproj
Root: HKA; Subkey: "Software\Classes\TonOfAble.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: tonproj
Root: HKA; Subkey: "Software\Classes\TonOfAble.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: tonproj

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,Ton of Able}"; Flags: nowait postinstall skipifsilent
