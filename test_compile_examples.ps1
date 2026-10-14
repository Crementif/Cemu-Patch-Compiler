param (
    [string]$CemuDir = "",
    [string]$CemuExecutable = "",
    [string]$GamePath = "",
    [string]$SourceDir = "",
    [string]$GraphicPackName = "PatchCompiler_Testing",
    [int]$BootTimeoutSeconds = 10,
    [int]$DurationSeconds = 5,
    [switch]$SkipCompile
)

$ErrorActionPreference = "Stop"

$RunAllExamples = [string]::IsNullOrWhiteSpace($SourceDir)

$WorkspaceRoot = $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($CemuDir)) {
    $CemuDir = Join-Path $WorkspaceRoot "testing"
}

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

function Write-Skip($Message) {
    Write-Host "[SKIP] $Message" -ForegroundColor Yellow
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

function Get-CemuSettingsPath {
    param([string]$BaseDir)

    $Candidates = @(
        (Join-Path $BaseDir "settings.xml"),
        (Join-Path $env:APPDATA "Cemu\settings.xml")
    )

    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            return $Candidate
        }
    }

    return $null
}

function Enable-CemuConsoleLogging {
    # OSReport only reaches log.txt when the CoreinitLogging flag (bit 17) is set
    param([string]$BaseDir)

    $SettingsPath = Get-CemuSettingsPath -BaseDir $BaseDir
    if ($null -eq $SettingsPath) {
        return $null
    }

    $OriginalContents = [System.IO.File]::ReadAllText($SettingsPath)
    if ($OriginalContents -notmatch '<logflag>(\d+)</logflag>') {
        return $null
    }

    $CoreinitLoggingFlag = [uint64]1 -shl 17
    $CurrentFlags = [uint64]$matches[1]
    if (($CurrentFlags -band $CoreinitLoggingFlag) -ne 0) {
        return $null
    }

    $UpdatedFlags = $CurrentFlags -bor $CoreinitLoggingFlag
    $UpdatedContents = $OriginalContents -replace '<logflag>\d+</logflag>', "<logflag>$UpdatedFlags</logflag>"
    [System.IO.File]::WriteAllText($SettingsPath, $UpdatedContents)

    return [pscustomobject]@{
        Path = $SettingsPath
        OriginalContents = $OriginalContents
    }
}

function Restore-CemuSettings {
    param($State)

    if ($null -eq $State) {
        return
    }

    [System.IO.File]::WriteAllText($State.Path, $State.OriginalContents)
}

function Get-ExpectedConsoleLines {
    param([string]$ExamplePath)

    $ExpectedFile = Join-Path $ExamplePath "expected_output.txt"
    if (-not (Test-Path $ExpectedFile)) {
        return @()
    }

    return @(Get-Content $ExpectedFile |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" -and -not $_.StartsWith("#") })
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
        (Join-Path $BaseDir "build\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "build\CemuPatchCompiler-debug.exe"),
        (Join-Path $BaseDir "build\bin\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "build\bin\CemuPatchCompiler-debug.exe"),
        (Join-Path $BaseDir "cmake-build-local-release\Release\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "cmake-build-release\Release\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "cmake-build-release\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "cmake-build-debug\Debug\CemuPatchCompiler-debug.exe"),
        (Join-Path $BaseDir "cmake-build-debug\CemuPatchCompiler-debug.exe"),
        (Join-Path $BaseDir "bin\CemuPatchCompiler-release.exe"),
        (Join-Path $BaseDir "bin\CemuPatchCompiler-debug.exe")
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
    $Process = Start-Process -FilePath $Executable -ArgumentList @("-g", $LaunchGamePath) -WorkingDirectory $BaseDir -WindowStyle Hidden -PassThru

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
    $Process = Start-Process -FilePath $Executable -ArgumentList @("-g", $LaunchGamePath) -WorkingDirectory $BaseDir -WindowStyle Hidden -PassThru

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

$CemuExe = $CemuExecutable
if ([string]::IsNullOrWhiteSpace($CemuExe)) {
    $CemuExe = Join-Path $CemuDir "Cemu.exe"
}
if (-not (Test-Path $CemuExe)) {
    Write-Failure "Cemu executable not found at $CemuExe"
    exit 1
}

$GraphicPackDir = Join-Path $CemuDir "graphicPacks\$GraphicPackName"
$RulesFile = Join-Path $GraphicPackDir "rules.txt"

$ExamplesToRun = @()
if ($RunAllExamples) {
    # Run all examples under the examples/ folder
    $ExamplesToRun = Get-ChildItem -Path (Join-Path $WorkspaceRoot "examples") -Directory | ForEach-Object { $_.FullName }
} else {
    $ExamplesToRun = @((Resolve-Path $SourceDir).Path)
}

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
version = 8
default = 1
"@

Set-Content -Path $RulesFile -Value $RulesContent -Encoding ascii
Write-Info "Rules file: $RulesFile"

$FailedExamples = @()
$SkippedExamples = @()
$ConsoleLoggingState = Enable-CemuConsoleLogging -BaseDir $CemuDir

foreach ($ExampleDir in $ExamplesToRun) {
    $ExampleName = Split-Path $ExampleDir -Leaf
    $RequiresEntryCallback = $false
    Write-Header "Testing Example: $ExampleName"

    # Clean the graphic pack directory of old asm files to prevent symbol duplication errors
    if (Test-Path $GraphicPackDir) {
        Remove-Item (Join-Path $GraphicPackDir "patch_*.asm") -Force -ErrorAction SilentlyContinue
    }
    # Copy any other patch files from the example directory
    Get-ChildItem -Path $ExampleDir -Filter "patch_*.asm" -File | Where-Object { $_.Name -ne "patch_compiled.asm" } | ForEach-Object {
        Copy-Item $_.FullName -Destination (Join-Path $GraphicPackDir $_.Name) -Force
    }
    # An example is compiled into patch_compiled.asm, or if it has patch_* folders, each folder into its own patch
    # file of the same graphic pack
    $PatchBuilds = @(Get-ChildItem -Path $ExampleDir -Directory -Filter "patch_*" | ForEach-Object {
        @{ Source = $_.FullName; Output = (Join-Path $GraphicPackDir "$($_.Name).asm") }
    })
    if ($PatchBuilds.Count -eq 0) {
        $PatchBuilds = @(@{ Source = $ExampleDir; Output = (Join-Path $GraphicPackDir "patch_compiled.asm") })
    }

    try {
        if (-not $SkipCompile) {
            $CompilerExe = Get-CompilerExe -BaseDir $WorkspaceRoot
            if ($null -eq $CompilerExe) {
                Write-Failure "Could not locate CemuPatchCompiler executable in the workspace bin or build folders."
                exit 1
            }

            Write-Info "Compiler: $CompilerExe"
            foreach ($PatchBuild in $PatchBuilds) {
                $PatchFile = $PatchBuild.Output
                Write-Info "Source path: $($PatchBuild.Source)"
                Write-Info "Output file: $PatchFile"

                $CompilerArgs = @($PatchBuild.Source, $PatchFile)

                $CompileProcess = Start-Process -FilePath $CompilerExe -ArgumentList $CompilerArgs -WorkingDirectory $WorkspaceRoot -NoNewWindow -PassThru -Wait
                if ($CompileProcess.ExitCode -ne 0) {
                    throw "Patch compilation failed with exit code $($CompileProcess.ExitCode)."
                }

                if (Test-Path $PatchFile) {
                    # Update moduleMatches checksum directly in the patch file to avoid mutating C++ files
                    $AsmContent = [System.IO.File]::ReadAllText($PatchFile)
                    $AsmContent = $AsmContent -replace '(?m)^moduleMatches\s*=\s*0x[0-9A-Fa-f]+', "moduleMatches = 0x$($Metadata.ModuleChecksum)"
                    [System.IO.File]::WriteAllText($PatchFile, $AsmContent, [System.Text.Encoding]::ASCII)
                } else {
                    throw "Compilation finished but output file not found at $PatchFile"
                }
            }

            Write-Success "Patch compiled successfully."
        } else {
            foreach ($PatchBuild in $PatchBuilds) {
                if (-not (Test-Path $PatchBuild.Output)) {
                    throw "SkipCompile was set but no compiled patch exists at $($PatchBuild.Output)."
                }
            }
        }

        $RequiresEntryCallback = @($PatchBuilds | Where-Object {
            [System.IO.File]::ReadAllText($_.Output) -match '(?m)^\.callback\s+entry\b'
        }).Count -gt 0

        Write-Info "Launching Homebrew with patch..."
        $LogLines = Invoke-CemuSmokeRun -BaseDir $CemuDir -Executable $CemuExe -LaunchGamePath $GamePath -Seconds $DurationSeconds

        $Errors = @()
        $AppliedAutoGenerated = $false
        foreach ($Line in $LogLines) {
            if ($Line -match "Applying patch group") {
                $AppliedAutoGenerated = $true
            }

            if ($Line -match "An error occurred while trying to apply the patches" -or
                $Line -match "Syntax error" -or
                $Line -match "Error in assembler" -or
                $Line -match "Error while processing" -or
                $Line -match "is already defined" -or
                $Line -match "No patches for this graphic pack will be applied") {
                $Errors += $Line
            }
        }

        if ($Errors.Count -gt 0) {
            $CallbackUnsupported = $RequiresEntryCallback -and
                (($Errors | Where-Object {
                    $_ -match "Instruction '\.CALLBACK' is unknown or not supported"
                }).Count -gt 0)
            if ($CallbackUnsupported) {
                Write-Skip "Example '$ExampleName' requires Cemu 2.7 or newer for .callback entry support."
                $SkippedExamples += $ExampleName
                continue
            }

            Write-Failure "Cemu reported patch application errors:"
            foreach ($ErrorLine in $Errors) {
                Write-Host "  $ErrorLine" -ForegroundColor Red
            }
            throw "Cemu assembler errors occurred."
        }

        if (-not $AppliedAutoGenerated) {
            throw "Cemu launched the title, but the generated PatchCompiler graphic pack was not applied."
        }

        $ExpectedLines = Get-ExpectedConsoleLines -ExamplePath $ExampleDir
        if ($ExpectedLines.Count -gt 0) {
            $LogText = $LogLines -join "`n"
            $MissingLines = @($ExpectedLines | Where-Object { -not $LogText.Contains($_) })
            if ($MissingLines.Count -gt 0) {
                Write-Failure "Cemu log is missing expected console output:"
                foreach ($MissingLine in $MissingLines) {
                    Write-Host "  $MissingLine" -ForegroundColor Red
                }

                $ConsoleLines = @($LogLines | Where-Object { $_ -match "\[OSConsole\]" })
                if ($ConsoleLines.Count -eq 0) {
                    Write-Info "The title did not write any console output. Enable Coreinit Logging in Cemu if this is unexpected."
                } else {
                    Write-Info "Console output that was produced:"
                    foreach ($ConsoleLine in ($ConsoleLines | Select-Object -First 25)) {
                        Write-Host "  $ConsoleLine" -ForegroundColor Gray
                    }
                    if ($ConsoleLines.Count -gt 25) {
                        Write-Host "  ... and $($ConsoleLines.Count - 25) more line(s)" -ForegroundColor Gray
                    }
                }

                throw "Expected console output was not produced."
            }

            Write-Info "Console output matched $($ExpectedLines.Count) expected line(s)."
        }

        Write-Success "Example '$ExampleName' verified successfully!"
    } catch {
        Write-Failure "Example '$ExampleName' FAILED: $_"
        $FailedExamples += $ExampleName
    }
}

Restore-CemuSettings -State $ConsoleLoggingState

if ($FailedExamples.Count -gt 0) {
    Write-Failure "Some examples failed verification: $($FailedExamples -join ', ')"
    exit 1
}

if ($SkippedExamples.Count -gt 0) {
    Write-Success "All supported examples verified successfully; skipped: $($SkippedExamples -join ', ')"
} else {
    Write-Success "All examples verified successfully!"
}
exit 0
