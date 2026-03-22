#pragma once

/*
 * indicator_config — Fetch configurazione dal proxy Mac via HTTP GET /config.
 *
 * config_fetch_from_proxy(): legge PROXY_IP/PORT da NVS, fa GET /config,
 * parsa il JSON ricevuto e salva ogni campo in NVS.
 *
 * indicator_config_init(): registra l'hook IP_EVENT_STA_GOT_IP per eseguire
 * il fetch automaticamente al primo collegamento Wi-Fi al boot.
 */

/* Esegue GET http://<proxy_ip>:<proxy_port>/config, parsa JSON, salva NVS.
 * Thread-safe; può essere chiamata da qualsiasi task.
 * Ritorna 0 su successo, -1 su errore. */
int config_fetch_from_proxy(void);

/* Registra l'event handler IP_EVENT_STA_GOT_IP per il fetch al boot.
 * Chiamare da indicator_model_init(), dopo indicator_wifi_init(). */
void indicator_config_init(void);
