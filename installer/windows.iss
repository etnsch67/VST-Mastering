[Setup]
AppName=Monster Clone
AppVersion=0.1.0
CreateAppDir=no
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=MonsterClone-Windows-Setup
Compression=lzma
SolidCompression=yes

[Files]
Source: "..\build\MonsterClone_artefacts\Release\VST3\Monster Clone.vst3\*"; DestDir: "{commoncf64}\VST3\Monster Clone.vst3"; Flags: recursesubdirs ignoreversion
