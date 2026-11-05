#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void stc_network_gate_enable_for_chat(void);
void stc_network_gate_disable(void);
bool stc_network_gate_is_enabled(void);

#ifdef __cplusplus
}
#endif
