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

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
        )
    }

    Remove-Item @RemoveArgs

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    $IsccFile = "${ProjectRoot}/build_${Target}/installer-Windows.generated.iss"

    if ( ! ( Test-Path -Path $IsccFile ) ) {
        throw 'InnoSetup install script not found. Run the build script or the CMake build and install procedures first.'
    }

    # Stage two layouts that each work on every OBS version we support.
    #
    # OBS 33 moved third-party plugins to <root>/plugins/<name>/<name>.dll with
    # data beside it, and stops loading the old locations in OBS 34. OBS 31 and
    # 32 only know the old ones. Each layout therefore carries the plugin in
    # both places: OBS 33+ loads the new copy and skips the old one by name,
    # older OBS never looks at the new one.
    #
    # ProgramData: goes in C:\ProgramData\obs-studio\plugins (installed OBS)
    #   <name>\<name>.dll                33+
    #   <name>\bin\64bit\<name>.dll      31 and 32
    #   <name>\data\                     shared by both
    #
    # Portable: goes in the OBS folder itself (portable OBS)
    #   plugins\<name>\<name>.dll + data\                 33+
    #   obs-plugins\64bit\<name>.dll + data\obs-plugins\  31 and 32
    $BuiltBin = "${ProjectRoot}/release/${Configuration}/${ProductName}/bin/64bit"
    $BuiltData = "${ProjectRoot}/release/${Configuration}/${ProductName}/data"

    $ProgramDataStage = "${ProjectRoot}/release/ProgramData"
    $PortableStage = "${ProjectRoot}/release/Portable"
    Remove-Item -Path $ProgramDataStage, $PortableStage, "${ProjectRoot}/release/OBS33", "${ProjectRoot}/release/OBS31-32", "${ProjectRoot}/release/WindowsAll" -Recurse -Force -ErrorAction SilentlyContinue

    $Targets = @(
        "${ProgramDataStage}/${ProductName}"
        "${ProgramDataStage}/${ProductName}/bin/64bit"
        "${PortableStage}/plugins/${ProductName}"
        "${PortableStage}/obs-plugins/64bit"
    )
    foreach ( $Target in $Targets ) {
        New-Item -ItemType Directory -Path $Target -Force | Out-Null
        Copy-Item -Path "${BuiltBin}/*" -Destination $Target -Recurse
    }

    if ( Test-Path -Path $BuiltData ) {
        Copy-Item -Path $BuiltData -Destination "${ProgramDataStage}/${ProductName}/data" -Recurse
        Copy-Item -Path $BuiltData -Destination "${PortableStage}/plugins/${ProductName}/data" -Recurse
        New-Item -ItemType Directory -Path "${PortableStage}/data/obs-plugins" -Force | Out-Null
        Copy-Item -Path $BuiltData -Destination "${PortableStage}/data/obs-plugins/${ProductName}" -Recurse
    }

    Copy-Item "${IsccFile}" -Destination "${ProjectRoot}/release"
    Copy-Item "${ProjectRoot}/media/icon.ico" -Destination "${ProjectRoot}/release"

    Log-Information 'Creating InnoSetup installer...'
    Push-Location -Stack BuildTemp
    Ensure-Location -Path "${ProjectRoot}/release"
    Invoke-External iscc ${IsccFile} /O. /F"${OutputName}-Installer"
    Pop-Location -Stack BuildTemp

    Log-Group "Staging manual install folders..."
    # One folder per manual download. CI uploads each folder as its own
    # artifact (GitHub zips it, so the folder's contents are the zip root) and
    # the release job zips them the same way.
    #   OBS33\plugins\<name>\         OBS 33+: extract into the OBS folder
    #                                 (portable) or C:\ProgramData\obs-studio
    #   OBS31-32\obs-plugins\64bit\   OBS 31 and 32: extract into the OBS folder
    #   OBS31-32\data\obs-plugins\
    $Obs33Stage = "${ProjectRoot}/release/OBS33"
    $LegacyStage = "${ProjectRoot}/release/OBS31-32"
    New-Item -ItemType Directory -Path $Obs33Stage, $LegacyStage -Force | Out-Null
    Copy-Item -Path "${PortableStage}/plugins" -Destination $Obs33Stage -Recurse
    Copy-Item -Path "${PortableStage}/obs-plugins" -Destination $LegacyStage -Recurse
    if ( Test-Path -Path "${PortableStage}/data" ) {
        Copy-Item -Path "${PortableStage}/data" -Destination $LegacyStage -Recurse
    }

    # Every Windows version in one download (the file the product pages link
    # to): the installer plus both manual install folders, extracted, so there
    # is never a zip inside a zip. No .pdb files, to keep the download small;
    # the separate OBS33 and OBS31-32 downloads keep them.
    #   WindowsAll\<name>-<ver>-windows-x64-Installer.exe
    #   WindowsAll\<name>-<ver>-windows-x64-obs33\plugins\...
    #   WindowsAll\<name>-<ver>-windows-x64-obs31-32\obs-plugins\... + data\...
    $AllStage = "${ProjectRoot}/release/WindowsAll"
    New-Item -ItemType Directory -Path $AllStage -Force | Out-Null
    Copy-Item -Path "${ProjectRoot}/release/${OutputName}-Installer.exe" -Destination $AllStage
    Copy-Item -Path $Obs33Stage -Destination "${AllStage}/${OutputName}-obs33" -Recurse
    Copy-Item -Path $LegacyStage -Destination "${AllStage}/${OutputName}-obs31-32" -Recurse
    Get-ChildItem -Path $AllStage -Recurse -Filter '*.pdb' | Remove-Item -Force

    Log-Group
}

Package
