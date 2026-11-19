$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot '..\main\servo_lid_test.c'
$source = Get-Content -LiteralPath $sourcePath -Raw

$expectedClose = 's_close_us\[SERVO_COUNT\]\s*=\s*\{\s*1450,\s*1450,\s*1450,\s*1450\s*\}'
$expectedOpen = 's_open_us\[SERVO_COUNT\]\s*=\s*\{\s*1575,\s*1575,\s*1575,\s*1575\s*\}'

if ($source -notmatch $expectedClose) {
    throw 'FAIL: expected all four lids to use a 1450 us close pulse'
}
if ($source -notmatch $expectedOpen) {
    throw 'FAIL: expected all four lids to use a 1575 us open pulse'
}
if ($source -notmatch 'command_open[^{]*\{[\s\S]*?s_open_us\[bin\]') {
    throw 'FAIL: open command does not use the per-bin open pulse'
}
if ($source -notmatch 'command_close[^{]*\{[\s\S]*?s_close_us\[bin\]') {
    throw 'FAIL: close command does not use the per-bin close pulse'
}

Write-Output 'PASS: calibrated pulse profiles are used for open and close'
