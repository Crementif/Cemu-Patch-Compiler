param (
    [string]$CemuDir = "",
    [string]$GamePath = "",
    [string]$SourceDir = "",
    [string]$GraphicPackName = "PatchCompiler_Testing",
    [int]$BootTimeoutSeconds = 10,
    [int]$DurationSeconds = 5,
    [switch]$SkipCompile
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($CemuDir)) {
    $CemuDir = $PSScriptRoot
}

$WorkspaceRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $WorkspaceRoot "examples\testing"
}

function Write-Header($Message) {
    Write-Host "`n=== $Message ===" -ForegroundColor Cyan
}

function Write-Info($Message) {
    Write-Host "[INFO] $Message" -ForegroundColor Gray
}

function Write-Success($Message) {
    Write-Host "[PASS] $Message" -ForegroundColor Green
}

function Write-Failure($Message) {
    Write-Host "[FAIL] $Message" -ForegroundColor Red
}

function Get-CemuLogCandidates {
    param([string]$BaseDir)

    return @(
        (Join-Path $BaseDir "log.txt"),
        (Join-Path $env:APPDATA "Cemu\log.txt")
    )
}

function Clear-CemuLogs {
    param([string]$BaseDir)

    foreach ($Path in (Get-CemuLogCandidates -BaseDir $BaseDir)) {
        if (Test-Path $Path) {
            Remove-Item $Path -Force -ErrorAction SilentlyContinue
        }
    }
}

function Get-CemuLogPath {
    param([string]$BaseDir)

    $Candidates = Get-CemuLogCandidates -BaseDir $BaseDir | Where-Object { Test-Path $_ }
    if ($Candidates.Count -eq 0) {
        return $null
    }

    return ($Candidates | Sort-Object { (Get-Item $_).LastWriteTimeUtc } -Descending | Select-Object -First 1)
}

function Get-HomebrewGamePath {
    param([string]$BaseDir)

    $Patterns = @("*.wuhb", "*.wua", "*.rpx", "*.elf", "*.wud", "*.wux", "*.iso")
    foreach ($Pattern in $Patterns) {
        $Match = Get-ChildItem -Path $BaseDir -Filter $Pattern -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $Match) {
            return $Match.FullName
        }
    }

    return $null
}

function Get-CompilerExe {
    param([string]$BaseDir)

    $Candidates = @(
        (Join-Path $BaseDir "cmake-build-local-release\Release\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "cmake-build-release\Release\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "cmake-build-debug\Debug\CemuPatchCompiler-debug.exe"),
        (Join-Path $BaseDir "bin\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "bin\CemuPatchCompiler-debug.exe"),
        (Join-Path $BaseDir "build\bin\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "build\bin\CemuPatchCompiler-debug.exe")
    )

    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return $Candidate
        }
    }

    return $null
}

function Set-SourceModuleMatches {
    param(
        [string]$SourcePath,
        [string]$Checksum
    )

    if (-not (Test-Path $SourcePath)) {
        throw "Source file not found at $SourcePath."
    }

    $OriginalContents = [System.IO.File]::ReadAllText($SourcePath)
    $NewLine = if ($OriginalContents.Contains("`r`n")) { "`r`n" } else { "`n" }
    $DirectiveLine = "// moduleMatches = 0x$Checksum"

    $WorkingContents = $OriginalContents
    if ($WorkingContents.Length -gt 0 -and $WorkingContents[0] -eq [char]0xFEFF) {
        $WorkingContents = $WorkingContents.Substring(1)
    }

    $FirstNewlineIndex = $WorkingContents.IndexOf("`n")
    if ($FirstNewlineIndex -ge 0) {
        $FirstLine = $WorkingContents.Substring(0, $FirstNewlineIndex).TrimEnd("`r")
        $RemainingContents = $WorkingContents.Substring($FirstNewlineIndex + 1)
    } else {
        $FirstLine = $WorkingContents.TrimEnd("`r")
        $RemainingContents = ""
    }

    if ($FirstLine -match '^\s*(#!|//\s*moduleMatches\b)') {
        $UpdatedContents = if ([string]::IsNullOrEmpty($RemainingContents)) {
            "$DirectiveLine$NewLine"
        } else {
            "$DirectiveLine$NewLine$RemainingContents"
        }
    } elseif ([string]::IsNullOrEmpty($WorkingContents)) {
        $UpdatedContents = "$DirectiveLine$NewLine"
    } else {
        $UpdatedContents = "$DirectiveLine$NewLine$WorkingContents"
    }

    [System.IO.File]::WriteAllText(
        $SourcePath,
        $UpdatedContents,
        [System.Text.Encoding]::ASCII
    )

    return $OriginalContents
}

function Get-LaunchMetadata {
    param([string]$LogPath)

    if (-not (Test-Path $LogPath)) {
        return $null
    }

    $TitleId = $null
    $ModuleChecksum = $null
    $ModuleName = $null

    foreach ($Line in (Get-Content $LogPath)) {
        if ($null -eq $TitleId -and $Line -match "TitleId:\s+([0-9A-Fa-f]{8})-([0-9A-Fa-f]{8})") {
            $TitleId = ($matches[1] + $matches[2]).ToUpperInvariant()
            continue
        }

        if ($null -eq $ModuleChecksum -and $Line -match "Loaded module '([^']+)' with checksum 0x([0-9A-Fa-f]{8})") {
            $ModuleName = $matches[1]
            $ModuleChecksum = $matches[2].ToUpperInvariant()
        }
    }

    if ($null -eq $TitleId -or $null -eq $ModuleChecksum) {
        return $null
    }

    return [pscustomobject]@{
        TitleId = $TitleId
        ModuleName = $ModuleName
        ModuleChecksum = $ModuleChecksum
    }
}

function Start-CemuAndWaitForMetadata {
    param(
        [string]$BaseDir,
        [string]$Executable,
        [string]$LaunchGamePath,
        [int]$TimeoutSeconds
    )

    Clear-CemuLogs -BaseDir $BaseDir
    $Process = Start-Process -FilePath $Executable -ArgumentList @("-g", $LaunchGamePath) -WorkingDirectory $BaseDir -PassThru

    try {
        $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        while ((Get-Date) -lt $Deadline) {
            $LogPath = Get-CemuLogPath -BaseDir $BaseDir
            if ($null -ne $LogPath) {
                $Metadata = Get-LaunchMetadata -LogPath $LogPath
                if ($null -ne $Metadata) {
                    return [pscustomobject]@{
                        Process = $Process
                        LogPath = $LogPath
                        Metadata = $Metadata
                    }
                }
            }

            Start-Sleep -Milliseconds 500
        }
    } finally {
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            $Process.WaitForExit(3000) | Out-Null
        }
    }

    return $null
}

function Invoke-CemuSmokeRun {
    param(
        [string]$BaseDir,
        [string]$Executable,
        [string]$LaunchGamePath,
        [int]$Seconds
    )

    Clear-CemuLogs -BaseDir $BaseDir
    $Process = Start-Process -FilePath $Executable -ArgumentList @("-g", $LaunchGamePath) -WorkingDirectory $BaseDir -PassThru

    try {
        Start-Sleep -Seconds $Seconds
    } finally {
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            $Process.WaitForExit(3000) | Out-Null
        }
    }

    $LogPath = Get-CemuLogPath -BaseDir $BaseDir
    if ($null -eq $LogPath) {
        throw "Cemu did not produce a log file."
    }

    return Get-Content $LogPath
}

if ([string]::IsNullOrWhiteSpace($GamePath)) {
    $GamePath = Get-HomebrewGamePath -BaseDir $CemuDir
}

if ([string]::IsNullOrWhiteSpace($GamePath) -or -not (Test-Path $GamePath)) {
    Write-Failure "Could not find a homebrew title in $CemuDir. Expected a .wuhb, .wua, .rpx, .elf, .wud, .wux, or .iso file."
    exit 1
}

$CemuExe = Join-Path $CemuDir "Cemu.exe"
if (-not (Test-Path $CemuExe)) {
    Write-Failure "Cemu executable not found at $CemuExe"
    exit 1
}

$GraphicPackDir = Join-Path $CemuDir "graphicPacks\$GraphicPackName"
$RulesFile = Join-Path $GraphicPackDir "rules.txt"
$SmokeSourceFile = Join-Path $SourceDir "smoke_test.cpp"
if (-not (Test-Path $SmokeSourceFile)) {
    $FallbackSourceFile = Get-ChildItem -Path $SourceDir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in @(".cpp", ".c", ".cc", ".cxx") } |
        Sort-Object Name |
        Select-Object -First 1
    if ($null -eq $FallbackSourceFile) {
        Write-Failure "Could not find a compilable smoke-test source file in $SourceDir."
        exit 1
    }

    $SmokeSourceFile = $FallbackSourceFile.FullName
}
$PatchFile = Join-Path $GraphicPackDir (([System.IO.Path]::GetFileNameWithoutExtension($SmokeSourceFile)) + ".asm")

Write-Header "Discovering Module Checksum"
Write-Info "Cemu directory: $CemuDir"
Write-Info "Game file: $GamePath"

$LaunchResult = Start-CemuAndWaitForMetadata -BaseDir $CemuDir -Executable $CemuExe -LaunchGamePath $GamePath -TimeoutSeconds $BootTimeoutSeconds
if ($null -eq $LaunchResult) {
    Write-Failure "Unable to discover title metadata from Cemu within $BootTimeoutSeconds seconds."
    exit 1
}

$Metadata = $LaunchResult.Metadata
Write-Success "Discovered title $($Metadata.TitleId) and module '$($Metadata.ModuleName)' checksum 0x$($Metadata.ModuleChecksum)."

Write-Header "Preparing Graphic Pack"
New-Item -ItemType Directory -Force -Path $GraphicPackDir | Out-Null

$RulesContent = @"
[Definition]
titleIds = $($Metadata.TitleId)
name = PatchCompiler Testing
path = "PatchCompiler/Testing"
description = Auto-generated local smoke test for CemuPatchCompiler.
version = 7
default = 1
"@

Set-Content -Path $RulesFile -Value $RulesContent -Encoding ascii
Write-Info "Rules file: $RulesFile"

$OriginalSmokeSourceContents = $null
try {
    if (-not $SkipCompile) {
        $CompilerExe = Get-CompilerExe -BaseDir $WorkspaceRoot
        if ($null -eq $CompilerExe) {
            Write-Failure "Could not locate CemuPatchCompiler executable in the workspace bin or build folders."
            exit 1
        }

        $OriginalSmokeSourceContents = Set-SourceModuleMatches -SourcePath $SmokeSourceFile -Checksum $Metadata.ModuleChecksum

        Write-Header "Compiling Smoke Patch"
        Write-Info "Compiler: $CompilerExe"
        Write-Info "Source file: $SmokeSourceFile"
        Write-Info "Output file: $PatchFile"

        $CompilerArgs = @($SmokeSourceFile, $PatchFile)

        $CompileProcess = Start-Process -FilePath $CompilerExe -ArgumentList $CompilerArgs -WorkingDirectory $WorkspaceRoot -NoNewWindow -PassThru -Wait
        if ($CompileProcess.ExitCode -ne 0) {
            Write-Failure "Patch compilation failed with exit code $($CompileProcess.ExitCode)."
            exit 1
        }

        Write-Success "Smoke patch compiled successfully."
    } elseif (-not (Test-Path $PatchFile)) {
        Write-Failure "SkipCompile was set but no compiled patch exists at $PatchFile."
        exit 1
    }

    Write-Header "Launching Homebrew With Patch"
    $LogLines = Invoke-CemuSmokeRun -BaseDir $CemuDir -Executable $CemuExe -LaunchGamePath $GamePath -Seconds $DurationSeconds

    $Errors = @()
    $AppliedAutoGenerated = $false
    foreach ($Line in $LogLines) {
        if ($Line -match "Applying patch group 'AutoGenerated'") {
            $AppliedAutoGenerated = $true
        }

        if ($Line -match "An error occurred while trying to apply the patches" -or
            $Line -match "Syntax error" -or
            $Line -match "Error in assembler" -or
            $Line -match "Error while processing" -or
            $Line -match "No patches for this graphic pack will be applied") {
            $Errors += $Line
        }
    }

    if ($Errors.Count -gt 0) {
        Write-Failure "Cemu reported patch application errors:"
        foreach ($ErrorLine in $Errors) {
            Write-Host "  $ErrorLine" -ForegroundColor Red
        }
        exit 1
    }

    if (-not $AppliedAutoGenerated) {
        Write-Failure "Cemu launched the title, but the generated PatchCompiler graphic pack was not applied."
        exit 1
    }

    Write-Success "Homebrew launched and the generated patch loaded without Cemu assembler errors."
    exit 0
} finally {
    if ($null -ne $OriginalSmokeSourceContents) {
        [System.IO.File]::WriteAllText(
            $SmokeSourceFile,
            $OriginalSmokeSourceContents,
            [System.Text.Encoding]::ASCII
        )
    }
}
