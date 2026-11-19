$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath (
    Join-Path $PSScriptRoot '..\main\app_main.c') -Raw
$cmake = Get-Content -LiteralPath (
    Join-Path $PSScriptRoot '..\main\CMakeLists.txt') -Raw

if ($source -match 'read\(STDIN_FILENO') {
    throw 'FAIL: console still depends on raw stdin reads'
}
if ($source -notmatch 'esp_console_new_repl_uart') {
    throw 'FAIL: UART console REPL is not initialized'
}
if ($source -notmatch 'esp_console_start_repl') {
    throw 'FAIL: UART console REPL is not started'
}
if ($cmake -notmatch '\bconsole\b') {
    throw 'FAIL: console component is missing from CMake requirements'
}

Write-Output 'PASS: monitor input uses the ESP-IDF UART console REPL'
