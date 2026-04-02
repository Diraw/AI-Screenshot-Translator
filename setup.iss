#define MyAppName "AI Screenshot Translator"
#define MyAppExeName "AI-Screenshot-Translator-Cpp.exe"

[Setup]
; 程序基本信息
AppName={#MyAppName}
AppVersion=1.0.5-beta
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
SetupIconFile={#SourcePath}\assets\icon.ico
UninstallDisplayName={#MyAppName}
OutputDir={#SourcePath}\setup
OutputBaseFilename=AI_Translator_Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; 开发者信息
AppPublisher=Diraw
AppPublisherURL=https://github.com/Diraw/AI-Screenshot-Translator
AppSupportURL=https://github.com/Diraw/AI-Screenshot-Translator/issues
; 版权信息
AppCopyright=Copyright © 2026 Diraw
; 如果已安装过，自动关闭正在运行的程序，防止文件占用导致安装失败
CloseApplications=yes
RestartApplications=no
; 允许用户在覆盖安装时无需先卸载
AllowNoIcons=yes
AppId={{c0b26d24-9393-41f3-815b-a13b15fe8f3e}}

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkablealone

[InstallDelete]
; Remove app-managed runtime files before copying new ones so renamed/removed
; payload files do not linger across upgrades. Do not touch user data files.
Type: filesandordirs; Name: "{app}\assets"
Type: filesandordirs; Name: "{app}\generic"
Type: filesandordirs; Name: "{app}\iconengines"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\networkinformation"
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\tls"
Type: files; Name: "{app}\D3Dcompiler_47.dll"
Type: files; Name: "{app}\opengl32sw.dll"
Type: files; Name: "{app}\profile.json"
Type: files; Name: "{app}\Qt6Core.dll"
Type: files; Name: "{app}\Qt6Gui.dll"
Type: files; Name: "{app}\Qt6Network.dll"
Type: files; Name: "{app}\Qt6Svg.dll"
Type: files; Name: "{app}\Qt6Widgets.dll"
Type: files; Name: "{app}\WebView2Loader.dll"
Type: files; Name: "{app}\wkf.log"
Type: files; Name: "{app}\debug.log"

[Files]
; Let Inno use normal version checks for versioned binaries.
Source: "{#SourcePath}\build\Release\{#MyAppExeName}"; DestDir: "{app}"
Source: "{#SourcePath}\build\Release\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs; Excludes: "{#MyAppExeName},storage,*.log,.stfolder,.stfolder\*"
; Preserve an existing user-maintained .env during upgrades.
Source: "{#SourcePath}\.env"; DestDir: "{app}"; Flags: onlyifdoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; 安装结束后询问是否立即启动程序
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\storage\*"
Type: filesandordirs; Name: "{app}\wkf.log"
Type: filesandordirs; Name: "{app}\debug.log"

[Code]
// 检查进程是否正在运行的函数
function IsAppRunning(const FileName: string): Boolean;
var
  FSWbemLocator: Variant;
  FWMIService: Variant;
  FWbemObjectSet: Variant;
begin
  Result := false;
  try
    FSWbemLocator := CreateOleObject('WbemScripting.SWbemLocator');
    FWMIService := FSWbemLocator.ConnectServer('', 'root\CIMV2');
    FWbemObjectSet := FWMIService.ExecQuery(Format('SELECT * FROM Win32_Process WHERE Name = "%s"', [FileName]));
    Result := (FWbemObjectSet.Count > 0);
  except
    // 如果 WMI 出错，保守起见返回 False 或记录日志
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ErrorCode: Integer;
begin
  // usAppFunctions 是准备开始执行卸载步骤
  if CurUninstallStep = usUninstall then
  begin
    if IsAppRunning('{#MyAppExeName}') then
    begin
      // 只有在运行的时候才调用强制杀进程
      Exec('taskkill.exe', '/f /im {#MyAppExeName}', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);
      Sleep(800); // 稍微多给一点点时间，确保 OS 释放文件句柄
    end;
  end;
end;
