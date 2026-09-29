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
    Remove-Item -Path $ProgramDataStage, $PortableStage -Recurse -Force -ErrorAction SilentlyContinue

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

    Log-Group "Archiving ${ProductName}..."
    # Extract into C:\ProgramData\obs-studio\plugins
    $CompressArgs = @{
        Path = (Get-ChildItem -Path $ProgramDataStage)
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}-programdata.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs

    # Extract into a portable OBS folder
    $CompressArgs = @{
        Path = (Get-ChildItem -Path $PortableStage)
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs

    Log-Group
}

Package
