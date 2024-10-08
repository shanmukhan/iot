#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_http_client.h"
#include "esp_camera.h"

#include "mqtt_client.h"

#include "mbedtls/base64.h"
// #include "esp_base64.h"

#include "time.h"

#define CAMERA_QUEUE_SIZE 20

#define HOST "20.219.36.78"
#define PORT "8081"

QueueHandle_t camera_queue;
QueueHandle_t mqtt_queue;

/** DEFINES **/
#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1
#define TCP_SUCCESS 1 << 0
#define TCP_FAILURE 1 << 1
#define MAX_FAILURES 10

/** GLOBALS **/

// event group to contain status information
static EventGroupHandle_t wifi_event_group;

// retry tracker
static int s_retry_num = 0;

// task tag
static const char *TAG = "WIFI";

#define BOARD_ESP32CAM_AITHINKER 1

// WROVER-KIT PIN Map
#ifdef BOARD_WROVER_KIT

#define CAM_PIN_PWDN -1  // power down is not used
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

bool SWITCH;
esp_mqtt_client_handle_t CLIENT;

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_SXGA,   // QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 10, // 0-63 lower number means higher quality
    .fb_count = 1       // if more than one, i2s runs in continuous mode. Use only with JPEG
};

static esp_err_t init_camera()
{
    // initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

esp_err_t client_event_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }

    return ESP_OK;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// event handler for wifi events
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Connecting to AP...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAX_FAILURES)
        {
            ESP_LOGI(TAG, "Reconnecting to AP...");
            esp_wifi_connect();
            s_retry_num++;
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
        }
    }
}

// event handler for ip events
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
    }
}

// connect to wifi and return the result
esp_err_t connect_wifi()
{
    int status = WIFI_FAILURE;

    /** INITIALIZE ALL THE THINGS **/
    // initialize the esp network interface
    ESP_ERROR_CHECK(esp_netif_init());

    // initialize default esp event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // create wifi station in the wifi driver
    esp_netif_create_default_wifi_sta();

    // setup wifi station with the default wifi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /** EVENT LOOP CRAZINESS **/
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &wifi_handler_event_instance));

    esp_event_handler_instance_t got_ip_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        &got_ip_event_instance));

    /** START THE WIFI DRIVER **/
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Subramanyam_2G",
            .password = "Sahasra089",
            // .ssid = "vedasaipriya",
            // .password = "9290427243",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };

    // set the wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "STA initialization complete");

    /** NOW WE WAIT **/
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_SUCCESS | WIFI_FAILURE,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_SUCCESS)
    {
        ESP_LOGI(TAG, "Connected to ap");
        status = WIFI_SUCCESS;
    }
    else if (bits & WIFI_FAILURE)
    {
        ESP_LOGI(TAG, "Failed to connect to ap");
        status = WIFI_FAILURE;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        status = WIFI_FAILURE;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    vEventGroupDelete(wifi_event_group);

    return status;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/switch", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        bool val = (strcmp(event->data, "true") == 0) ? false : true;
        printf("SWITCH=%d val=%d \n", SWITCH, val);
        SWITCH = !SWITCH;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://20.219.36.78",
        // .broker.address.uri = "mqtt://192.168.29.242",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    CLIENT = client;
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void camera_task(void *pvParameters)
{
    printf("Camera task created.");
    int i = 1;
    while (1)
    {

        if (SWITCH)
        {
            i++;
            printf("#");
            camera_fb_t *pic = esp_camera_fb_get();

            uint8_t *output = calloc((pic->len + 2 - ((pic->len + 2) % 3)) / 3 * 4 + 1, sizeof(char));
            // <--- equation from internet
            size_t outlen = 0;
            int err = mbedtls_base64_encode(output, (pic->len + 2 - ((pic->len + 2) % 3)) / 3 * 4 + 1,
                                            &outlen, pic->buf, pic->len);

            // printf("Size of pic is %d bytes and base64 is %u %u %d \n", pic->len, sizeof(output), sizeof(*output), strlen((char *)output));

            xQueueSend(camera_queue, &output, 0);

            esp_camera_fb_return(pic);
            pic = NULL;
            // free(output);  // this is done at mqtt publish task
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        else
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    // printf("Exiting camera task");
    // vTaskDelete(NULL);
}

void mqtt_publish_task(void *pvParameters)
{
    printf("mqtt publish task created.");

    uint8_t *frame;
    while (1)
    {
        printf("*");
        if (xQueueReceive(camera_queue, &frame, portMAX_DELAY))
        {
            clock_t start = clock();

            int msg_id = esp_mqtt_client_enqueue(CLIENT, "/live", (char *)frame, 0, 1, 0, 0);
            // int msg_id = esp_mqtt_client_publish(CLIENT, "/live", (char *)frame, 0, 0, 0);
            int size = strlen((char *)frame);
            free(frame);

            clock_t end = clock();

            // Calculate the elapsed time in seconds
            double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;

            // Print the elapsed time
            printf("Time taken to send to mqtt: %.2f seconds\n", elapsed_time);

            ESP_LOGI(TAG, "sent publish successful, msg_id=%d %.2f seconds\n", msg_id, elapsed_time);
        }
    }
}

void socket_client_task(void *pvParameters)
{
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    struct addrinfo *result;
    if (getaddrinfo(HOST, PORT, &hints, &result) != 0)
    {
        perror("getaddrinfo");
    }

    int sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd == -1)
    {
        printf("Unable to create socket");
        perror("socket");
    }

    if (connect(sockfd, result->ai_addr, result->ai_addrlen) == -1)
    {
        printf("Unable to connect to socket");
        perror("connect");
        close(sockfd);
    }

    uint8_t *frame;

    while (1)
    {
        printf("*");
        if (xQueueReceive(camera_queue, &frame, portMAX_DELAY))
        {
            // Send a message to the server
            clock_t start = clock();
            int len = send(sockfd, (void *)frame, strlen((char *)frame), 0);
            clock_t end = clock();

            if (len < 0)
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            }

            double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;

            // Print the elapsed time
            printf("Time taken to send to mqtt: %.2f seconds\n", elapsed_time);
            ESP_LOGI(TAG, "sent publish successful size %d", strlen((char *)frame));

            free(frame);
        }
    }
    // Close the socket
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Wait for 2 seconds before trying to reconnect

    vTaskDelete(NULL);
}

void app_main(void)
{

    init_camera();

    esp_err_t status = WIFI_FAILURE;

    // initialize storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // connect to wireless AP
    status = connect_wifi();
    if (WIFI_SUCCESS != status)
    {
        ESP_LOGI(TAG, "Failed to associate to AP, dying...");
        return;
    }

    mqtt_app_start();

    printf("starting mqtt testing\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    // create camera queue
    camera_queue = xQueueCreate(CAMERA_QUEUE_SIZE, sizeof(camera_fb_t *));

    // create camera task
    xTaskCreate(&camera_task, "camera_task", 4096, NULL, 5, NULL);
    // create MQTT publish task
    xTaskCreate(&mqtt_publish_task, "mqtt_publish_task", 4096, NULL, 5, NULL);

    // xTaskCreate(&socket_client_task, "socket_client_task", 4096, NULL, 5, NULL);
}