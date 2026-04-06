#pragma once

/* Registra handler IP_EVENT_STA_GOT_IP.
 * Al boot (con 5s delay dopo il config fetch), fa GET /api/4/system
 * da Glances, parsa "hostname" e lo salva in NVS_KEY_GLANCES_HOST ("glances_host").
 * Non tocca NVS_KEY_SERVER_NAME ("srv_name") — nome display esclusivo di Settings/proxy. */
void indicator_system_init(void);
