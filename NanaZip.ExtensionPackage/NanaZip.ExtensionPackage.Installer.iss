﻿#define AppName "NanaZip Extension Package"
#define AppPublisher "M2-Team"
#define AppCopyright "© M2-Team and Contributors. All rights reserved."
#define AppURL "https://github.com/M2Team/NanaZip"

#ifndef AppVersion
#define AppVersion "5.1.0.0"
#endif

#define ShellextName "NanaZip.ExtensionPackage.Shell"
#define ShellextClsid "{{542CE69A-6EA7-4D77-9B8F-8F56CEA2BF16}"

[Setup]
AppId={{42795434-AB1A-4197-A724-F13E08953DFC}
AppName={#AppName}
AppCopyright={#AppCopyright}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DisableDirPage=yes
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
VersionInfoVersion={#AppVersion}

PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputBaseFilename=NanaZip.ExtensionPackage_{#AppVersion}
SolidCompression=yes
WizardStyle=modern

ArchitecturesAllowed=x64os or arm64
ArchitecturesInstallIn64BitMode=x64os or arm64

#ifdef InputPath
SourceDir="{#InputPath}"
#endif

[Files]
Source: "x64\{#ShellextName}.dll"; DestDir: {app}; DestName: "{#ShellextName}.x64.dll"; Flags: 64bit uninsrestartdelete; Check: IsX64OS
Source: "ARM64\{#ShellextName}.dll"; DestDir: {app}; DestName: "{#ShellextName}.ARM64.dll"; Flags: 64bit uninsrestartdelete; Check: IsArm64

[Registry]
Root: HKA; Subkey: "Software\Classes\CLSID\{#ShellextClsid}"; Flags: uninsdeletekeyifempty

Root: HKA; Subkey: "Software\Classes\CLSID\{#ShellextClsid}\InprocServer32"; ValueType: string; ValueData: "{app}\{#ShellextName}.x64.dll"; Flags: uninsdeletevalue uninsdeletekeyifempty; Check: IsX64OS
Root: HKA; Subkey: "Software\Classes\CLSID\{#ShellextClsid}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"; Flags: uninsdeletevalue uninsdeletekeyifempty; Check: IsX64OS

Root: HKA; Subkey: "Software\Classes\CLSID\{#ShellextClsid}\InprocServer32"; ValueType: string; ValueData: "{app}\{#ShellextName}.ARM64.dll"; Flags: uninsdeletevalue uninsdeletekeyifempty; Check: IsArm64
Root: HKA; Subkey: "Software\Classes\CLSID\{#ShellextClsid}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"; Flags: uninsdeletevalue uninsdeletekeyifempty; Check: IsArm64

Root: HKA; Subkey: "Software\Classes\Directory\shellex\CopyHookHandlers\{#ShellextClsid}"; ValueType: string; ValueData: "{#ShellextClsid}"; Flags: uninsdeletevalue uninsdeletekeyifempty

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
