#pragma once

// The STC shell uses the same verified Korvo-1 wiring as the upstream board.
// Keeping one hardware source of truth prevents the GAME/CHAT variant drifting
// away from the working codec, display, touch and camera pin map.
#include "../esp32-s31-korvo-1/config.h"
