; Installs the BILIBILI Stream for OBS plugin into either a regular OBS
; installation (ProgramData) or a portable OBS folder chosen by the user.
;
; Both plugin layouts are installed so the same package works on OBS 30-34:
;   <app>\<plugin>.dll             OBS 33+ layout
;   <app>\bin\64bit\<plugin>.dll   OBS 30-32 layout (skipped by OBS 33+)
;
; The defines below are normally supplied by ISCC /D at build time.

#ifndef PluginName
  #define PluginName "bilibili-stream-for-obs"
#endif

#ifndef PluginVersion
  #define PluginVersion "0.0.0"
#endif

#ifndef AppVersion
  #define AppVersion PluginVersion
#endif

#ifndef AppDisplayName
  #define AppDisplayName "BILIBILI Stream for OBS"
#endif

#ifndef PluginAuthor
  #define PluginAuthor "Zaros"
#endif

#ifndef Configuration
  #define Configuration "RelWithDebInfo"
#endif

#define PluginDllName PluginName + ".dll"

[Setup]
AppId={{B7C4E9A2-3F6D-4A1B-8E5C-9D2F7A6B1C34}
AppName={#AppDisplayName}
AppVersion={#AppVersion}
AppVerName={#AppDisplayName} {#PluginVersion}
AppPublisher={#PluginAuthor}
AppPublisherURL=https://github.com/Zarosmm/obs-bilibili-stream
AppSupportURL=https://github.com/Zarosmm/obs-bilibili-stream/issues
AppUpdatesURL=https://github.com/Zarosmm/obs-bilibili-stream/releases
DefaultDirName={code:GetDefaultDirName}
DefaultGroupName={#AppDisplayName}
DisableProgramGroupPage=yes
DisableDirPage=no
UsePreviousAppDir=no
OutputDir=..\..\release
OutputBaseFilename={#PluginName}-{#PluginVersion}-windows-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayName={#AppDisplayName}
UninstallDisplayIcon={app}\{#PluginDllName}
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
CloseApplicationsFilter=*.dll,*.exe

[Files]
Source: "..\..\release\{#Configuration}\{#PluginName}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\release\{#Configuration}\{#PluginName}\bin\64bit\{#PluginDllName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\release\{#Configuration}\{#PluginName}\bin\64bit\{#PluginDllName}"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\data"
Type: dirifempty; Name: "{app}"

[Code]
var
  ModePage: TInputOptionWizardPage;
  PortablePage: TInputDirWizardPage;
  PortableMode: Boolean;

function StripOuterQuotes(const S: String): String;
begin
  Result := S;
  if (Length(Result) >= 2) and (Result[1] = '"') and (Result[Length(Result)] = '"') then
    Result := Copy(Result, 2, Length(Result) - 2);
end;

function NormalizeRoot(const Root: String): String;
begin
  Result := Trim(StripOuterQuotes(Root));
  while (Length(Result) > 0) and (Result[Length(Result)] = '\') do
    Delete(Result, Length(Result), 1);
end;

function GetPortableParam: String;
var
  I: Integer;
  P: String;
begin
  Result := '';
  for I := 1 to ParamCount do
  begin
    P := ParamStr(I);
    if Copy(UpperCase(P), 1, 10) = '/PORTABLE=' then
    begin
      Result := NormalizeRoot(Copy(P, 11, Length(P) - 10));
      Exit;
    end;
  end;
end;

function PortableDestination(const Root: String): String;
begin
  Result := AddBackslash(NormalizeRoot(Root)) + 'plugins\{#PluginName}';
end;

function IsValidOBSRoot(const Root: String): Boolean;
var
  Normalized: String;
begin
  Normalized := NormalizeRoot(Root);
  Result := (Normalized <> '') and FileExists(AddBackslash(Normalized) + 'bin\64bit\obs64.exe');
end;

function IsUnderProgramFiles(const Root: String): Boolean;
var
  Normalized: String;
begin
  Normalized := UpperCase(AddBackslash(NormalizeRoot(Root)));
  Result := (Pos(UpperCase(ExpandConstant('{commonpf}\')), Normalized) = 1) or
            (Pos(UpperCase(ExpandConstant('{commonpf32}\')), Normalized) = 1);
end;

function GetDefaultDirName(Param: String): String;
begin
  Param := GetPortableParam;
  if Param <> '' then
    Result := PortableDestination(Param)
  else
    Result := ExpandConstant('{commonappdata}\obs-studio\plugins\{#PluginName}');
end;

function InitializeSetup: Boolean;
var
  Root: String;
begin
  Root := GetPortableParam;
  PortableMode := Root <> '';
  Result := True;
  if PortableMode then
  begin
    if not IsValidOBSRoot(Root) then
    begin
      SuppressibleMsgBox('The /PORTABLE path is not a valid OBS Studio folder: bin\64bit\obs64.exe was not found.', mbError, MB_OK, IDOK);
      Result := False;
    end
    else if IsUnderProgramFiles(Root) then
    begin
      SuppressibleMsgBox('The /PORTABLE path is under Program Files. Run the installer without /PORTABLE for a regular OBS installation.', mbError, MB_OK, IDOK);
      Result := False;
    end;
  end;
end;

procedure InitializeWizard;
var
  I: Integer;
  Candidates: array[0..2] of String;
begin
  ModePage := CreateInputOptionPage(wpWelcome,
    'Installation type',
    'How is OBS Studio installed on this computer?',
    'Choose where the plugin files should be copied.',
    True, False);
  ModePage.Add('Installed OBS Studio (all users, ProgramData)');
  ModePage.Add('Portable OBS Studio (select the OBS root folder)');
  ModePage.Values[0] := not PortableMode;
  ModePage.Values[1] := PortableMode;

  PortablePage := CreateInputDirPage(ModePage.ID,
    'Portable OBS folder',
    'Select the portable OBS Studio root folder.',
    'The folder must contain bin\64bit\obs64.exe.',
    False, '');
  PortablePage.Add('OBS root folder:');

  if PortableMode then
  begin
    PortablePage.Values[0] := GetPortableParam;
    WizardForm.DirEdit.Text := PortableDestination(GetPortableParam);
  end
  else
  begin
    Candidates[0] := ExpandConstant('{sd}\obs-studio');
    Candidates[1] := ExpandConstant('{%USERPROFILE}\obs-studio');
    Candidates[2] := ExpandConstant('{userdocs}\obs-studio');
    for I := 0 to 2 do
      if IsValidOBSRoot(Candidates[I]) then
      begin
        PortablePage.Values[0] := Candidates[I];
        Break;
      end;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = PortablePage.ID) and (not ModePage.Values[1]);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = ModePage.ID then
  begin
    if not ModePage.Values[1] then
      WizardForm.DirEdit.Text := ExpandConstant('{commonappdata}\obs-studio\plugins\{#PluginName}');
  end
  else if CurPageID = PortablePage.ID then
  begin
    if not IsValidOBSRoot(PortablePage.Values[0]) then
    begin
      MsgBox('The selected folder is not a valid OBS Studio folder.' + #13#10 + #13#10 + 'Select the OBS root folder that contains bin\64bit\obs64.exe.', mbError, MB_OK);
      Result := False;
      Exit;
    end;
    if IsUnderProgramFiles(PortablePage.Values[0]) then
    begin
      MsgBox('This folder is under Program Files and looks like a regular OBS installation.' + #13#10 + #13#10 + 'Go back and choose "Installed OBS Studio" instead.', mbError, MB_OK);
      Result := False;
      Exit;
    end;
    WizardForm.DirEdit.Text := PortableDestination(PortablePage.Values[0]);
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectDir then
  begin
    WizardForm.DirEdit.ReadOnly := True;
    WizardForm.DirBrowseButton.Enabled := False;
  end;
end;
