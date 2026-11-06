[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

function Find-InnoCompiler {
    $Candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )

    foreach ( $Candidate in $Candidates ) {
        if ( $Candidate -and ( Test-Path $Candidate ) ) {
            return $Candidate
        }
    }

    $Command = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
    if ( $Command ) {
        return $Command.Source
    }

    return $null
}

function Install-InnoCompiler {
    if ( ! ( Get-Command 'choco.exe' -ErrorAction SilentlyContinue ) ) {
        throw 'Inno Setup is not installed and Chocolatey is unavailable to install it.'
    }

    Write-Information 'Installing Inno Setup via Chocolatey...'
    & choco install innosetup --yes --no-progress
    if ( $LASTEXITCODE -ne 0 ) {
        throw "choco install innosetup failed with exit code $LASTEXITCODE"
    }
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"
    $InstallerScript = "${ProjectRoot}/.github/installer/bilibili-stream-for-obs.iss"
    $StagedRoot = "${ProjectRoot}/release/${Configuration}/"

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version
    $DisplayName = $BuildSpec.displayName
    $Author = $BuildSpec.author

    if ( ! $DisplayName ) {
        $DisplayName = $ProductName
    }

    if ( ! $Author ) {
        $Author = $ProductName
    }

    if ( "$DisplayName$Author".Contains('"') ) {
        throw 'buildspec.json displayName/author must not contain double quotes.'
    }

    if ( ! ( Test-Path $InstallerScript ) ) {
        throw "Installer script not found: $InstallerScript"
    }

    $PluginSource = "${StagedRoot}${ProductName}"
    $RequiredPaths = @(
        "${PluginSource}/bin/64bit/${ProductName}.dll"
        "${PluginSource}/data"
    )

    foreach ( $RequiredPath in $RequiredPaths ) {
        if ( ! ( Test-Path $RequiredPath ) ) {
            throw "Missing staged plugin file: $RequiredPath"
        }
    }

    $InnoCompiler = Find-InnoCompiler
    if ( ! $InnoCompiler ) {
        Install-InnoCompiler
        $InnoCompiler = Find-InnoCompiler
    }

    if ( ! $InnoCompiler ) {
        throw 'Inno Setup compiler (ISCC.exe) could not be found after installation.'
    }

    # Inno expects a numeric version for AppVersion; prerelease suffixes in the
    # build spec (for example 2.1.6-rc1) are trimmed for it.
    $AppVersion = if ( $ProductVersion -match '^([0-9]+(\.[0-9]+){0,3})' ) { $Matches[1] } else { $ProductVersion }

    $InstallerArgs = @(
        '/Qp'
        "/DPluginName=${ProductName}"
        "/DPluginVersion=${ProductVersion}"
        "/DAppVersion=${AppVersion}"
        "/DAppDisplayName=${DisplayName}"
        "/DPluginAuthor=${Author}"
        "/DConfiguration=${Configuration}"
        $InstallerScript
    )

    Write-Information "Building installer with ${InnoCompiler}..."
    & $InnoCompiler @InstallerArgs
    if ( $LASTEXITCODE -ne 0 ) {
        throw "Inno Setup compiler failed with exit code $LASTEXITCODE"
    }

    $InstallerPath = "${ProjectRoot}/release/${ProductName}-${ProductVersion}-windows-${Target}.exe"
    if ( ! ( Test-Path $InstallerPath ) ) {
        throw "Installer was not created: $InstallerPath"
    }

    $Installer = Get-Item $InstallerPath
    $Hash = ( Get-FileHash -Path $Installer.FullName -Algorithm SHA256 ).Hash.ToLower()
    Write-Information "Installer: $($Installer.Name) ($($Installer.Length) bytes)"
    Write-Information "SHA256: $Hash"
}

Package
