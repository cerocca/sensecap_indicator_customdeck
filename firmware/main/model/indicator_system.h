#pragma once

/* Registra handler IP_EVENT_STA_GOT_IP.
 * Al boot (con 5s delay dopo il config fetch), fa GET /api/4/system
 * da Glances, parsa "hostname" e lo salva in NVS come srv_name.
 * Il valore automatico sovrascrive quello manuale impostato in Settings. */
void indicator_system_init(void);
