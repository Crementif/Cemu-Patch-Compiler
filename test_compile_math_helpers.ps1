param(
    [Parameter(Mandatory = $true)]
    [string]$Compiler,
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"
$WorkspaceRoot = $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $WorkspaceRoot "build\math_helpers_test.asm"
}

& $Compiler (Join-Path $WorkspaceRoot "examples\math_test") -o $Output
if ($LASTEXITCODE -ne 0) {
    throw "Math helper patch compilation failed with exit code $LASTEXITCODE."
}

$Assembly = [System.IO.File]::ReadAllText($Output)
$RequiredSymbols = @(
    "patchMathSqrtf",
    "patchMathBuiltinSqrtf",
    "patchMathSqrt",
    "patchMathBuiltinSqrt",
    "patchMathFastSqrtf",
    "patchMathFastSqrt",
    "patchMathSinf",
    "patchMathCosf",
    "patchMathTanf",
    "patchMathFabsf",
    "patchMathCopysignf",
    "patchMathFminf",
    "patchMathFmaxf",
    "patchMathTruncf",
    "patchMathFloorf",
    "patchMathCeilf",
    "patchMathRoundf",
    "patchMathGenerate",
    "patchMathSelfTest"
)

foreach ($Symbol in $RequiredSymbols) {
    if ($Assembly -notmatch "(?m)^$([regex]::Escape($Symbol)):$") {
        throw "Generated patch is missing required symbol '$Symbol'."
    }
}

if ($Assembly -match '(?m)^\s+bl\s+(sqrtf?|sinf|cosf|tanf|fabsf|copysignf|fminf|fmaxf|truncf|floorf|ceilf|roundf)\s*$') {
    throw "Generated patch contains an unresolved math runtime call: $($matches[0].Trim())"
}

if ($Assembly -match '\bfsqrts?\b') {
    throw "Generated patch contains unsupported fsqrt/fsqrts instructions."
}

if ($Assembly -match '(?m)^\s+b(nu|un)\b') {
    throw "Generated patch contains BNU/BUN aliases unsupported by Cemu's assembler."
}

if ($Assembly -notmatch '\bfrsqrte\b') {
    throw "Generated patch does not contain the expected frsqrte estimate instruction."
}

if ($Assembly -notmatch '(?m)^\.callback entry _[0-9a-f]{16}__cemu_patch_entry[0-9]+$') {
    throw "Generated patch does not contain the expected entry callback directive."
}

if ($Assembly -notmatch '(?m)^_[0-9a-f]{16}__cemu_patch_entry[0-9]+:$') {
    throw "Generated patch does not contain the generated entry callback trampoline."
}

Write-Host "Math helper compile test passed: $Output"
