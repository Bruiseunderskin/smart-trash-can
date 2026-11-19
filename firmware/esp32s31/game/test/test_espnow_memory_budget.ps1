$ErrorActionPreference = 'Stop'

$root = Join-Path $PSScriptRoot '..'
$defaults = Get-Content -LiteralPath (Join-Path $root 'sdkconfig.defaults') -Raw
$sdkconfig = Get-Content -LiteralPath (Join-Path $root 'sdkconfig') -Raw
$source = Get-Content -LiteralPath (
    Join-Path $root 'main\espnow_controller.c') -Raw
$app = Get-Content -LiteralPath (
    Join-Path $root 'main\app_main.cpp') -Raw

$required = @(
    'CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y',
    'CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=2',
    'CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=4',
    'CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM=4',
    'CONFIG_ESP_WIFI_CACHE_TX_BUFFER_NUM=0',
    'CONFIG_ESP_WIFI_RX_MGMT_BUF_NUM_DEF=2',
    'CONFIG_ESP_WIFI_MGMT_SBUF_NUM=8',
    'CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=0'
)

foreach ($setting in $required) {
    if ($defaults -notmatch [regex]::Escape($setting)) {
        throw "FAIL: missing default $setting"
    }
    if ($sdkconfig -notmatch [regex]::Escape($setting)) {
        throw "FAIL: active sdkconfig missing $setting"
    }
}

if ($defaults -notmatch 'CONFIG_ESP_WIFI_ENABLE_WPA3_SAE=n') {
    throw 'FAIL: WPA3 is still enabled in defaults'
}
if ($sdkconfig -match 'CONFIG_ESP_WIFI_ENABLE_WPA3_SAE=y') {
    throw 'FAIL: WPA3 is still enabled in active sdkconfig'
}

if ($source -match 'ESP_ERROR_CHECK\s*\(\s*esp_wifi_init') {
    throw 'FAIL: Wi-Fi allocation failure still aborts the firmware'
}
if ($source -notmatch 'return\s+STC_ERR_IO') {
    throw 'FAIL: ESP-NOW init does not propagate allocation failure'
}
if ($app -match '#if\s+STC_GAME_LIVE_CAMERA\s+&&\s+!STC_GAME_TOUCH_BIN[\s\S]{0,80}camera_smoke_test') {
    throw 'FAIL: normal game consumes camera memory before ESP-NOW starts'
}

Write-Output 'PASS: ESP-NOW uses a bounded, recoverable memory budget'
