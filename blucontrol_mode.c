#include "blucontrol_mode.h"

#define LOG_TAG "BLU_MODE"

int mode_leds[MODE_LENGTH] =
{
    MODE1_LED_PIN,
#if MODE_LENGTH > 1
    MODE2_LED_PIN,
#endif
#if MODE_LENGTH > 2
    MODE3_LED_PIN,
#endif
#if MODE_LENGTH > 3
    MODE4_LED_PIN,
#endif
#if MODE_LENGTH > 4
    MODE5_LED_PIN,
#endif
#if MODE_LENGTH > 5
    MODE6_LED_PIN,
#endif
#if MODE_LENGTH > 6
    MODE7_LED_PIN,
#endif
#if MODE_LENGTH > 7
    MODE8_LED_PIN,
#endif
#if MODE_LENGTH > 8
    MODE9_LED_PIN,
#endif
#if MODE_LENGTH > 9
    MODE10_LED_PIN,
#endif
};

int switch_buttons[SWITCH_BUTTONS_LENGTH] = 
{
    SWITCH_BUTTON1_PIN,
#if SWITCH_BUTTONS_LENGTH > 1
    SWITCH_BUTTON2_PIN,
#endif
#if SWITCH_BUTTONS_LENGTH > 2
    SWITCH_BUTTON3_PIN,
#endif
#if SWITCH_BUTTONS_LENGTH > 3
    SWITCH_BUTTON4_PIN,
#endif
};
bool ignored_switch_buttons[SWITCH_BUTTONS_LENGTH] = 
{
    false,
#if SWITCH_BUTTONS_LENGTH > 1
    false,
#endif
#if SWITCH_BUTTONS_LENGTH > 2
    false,
#endif
#if SWITCH_BUTTONS_LENGTH > 3
    false,
#endif
};

bool blucontrol_handle_buttons_with_ota(bool with_ota);
void buttons_loop(void *obj);
void switch_mode_task(void *obj);
void switch_ota_task(void *obj);

int currentMode = -1;
TaskHandle_t buttonsLoopTaskHandle = NULL;
bool has_ota = true;

void blucontrol_mode_init(bool _has_ota)
{
    has_ota = _has_ota;
    const esp_partition_t* currentPartition = esp_ota_get_boot_partition();
    currentMode =  currentPartition->subtype - (ESP_PARTITION_SUBTYPE_APP_OTA_MIN + has_ota);
    for (int i = 0; i < MODE_LENGTH; i++)
    {
        if (mode_leds[i] < 0)
        {
            continue;
        }
        gpio_reset_pin(mode_leds[i]);
        gpio_set_direction(mode_leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(mode_leds[i], i == currentMode ? LED_POWER_ON : !LED_POWER_ON);
    }

    while(has_ota && blucontrol_handle_buttons_with_ota(true))
    {
        vTaskDelay(0.2 / portTICK_PERIOD_MS);
    }

    xTaskCreatePinnedToCore(buttons_loop, "BLU_MODE_LOOP", 1024, NULL, tskIDLE_PRIORITY, &buttonsLoopTaskHandle, 1);
    configASSERT(buttonsLoopTaskHandle);
}

uint32_t last_tick = 0;
bool blucontrol_handle_buttons_with_ota(bool with_ota)
{
    bool all_pressed = true;
    for (int i = 0; i < SWITCH_BUTTONS_LENGTH; i++)
    {
        if (switch_buttons[i] < 0)
        {
            if (!ignored_switch_buttons[i])
            {
                ESP_LOGW(LOG_TAG, "Switch Button %d has a non-positive value. Ignoring this button.", i + 1);
                ignored_switch_buttons[i] = true;
            }
            if (SWITCH_BUTTONS_LENGTH == 1)
            {
                all_pressed = false;
            }
            continue;
        }

        if (gpio_get_level(switch_buttons[i]) != BUTTONS_PRESS_STATE)
        {
            all_pressed = false;
            break;
        }
    }

    if (all_pressed)
    {
        if (last_tick == 0)
        {
            last_tick = xTaskGetTickCount();
        }
        else if (xTaskGetTickCount() - last_tick >= SECONDS_TO_SWITCH * 1000)
        {
            last_tick = 0;
            if (with_ota)
            {
                blucontrol_switch_ota_mode();
            }
            else
            {
                blucontrol_switch_mode();
            }
        }
    }
    else
    {
        last_tick = 0;
    }

    return all_pressed;
}

bool blucontrol_handle_buttons(void)
{
    return blucontrol_handle_buttons_with_ota(false);
}

TaskHandle_t switchModeTaskHandle = NULL;
void blucontrol_switch_mode(void)
{
    xTaskCreatePinnedToCore(switch_mode_task, "BLU_SWITCH_MODE", 4096, NULL, tskIDLE_PRIORITY, &switchModeTaskHandle, 1);
    configASSERT(switchModeTaskHandle);
}

TaskHandle_t switchOTATaskHandle = NULL;
void blucontrol_switch_ota_mode(void)
{
    xTaskCreatePinnedToCore(switch_ota_task, "BLU_SWITCH_OTA", 4096, NULL, tskIDLE_PRIORITY, &switchOTATaskHandle, 1);
    configASSERT(switchOTATaskHandle);
}

void buttons_loop(void *obj)
{
    while(true)
    {
        blucontrol_handle_buttons();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void switch_mode_task(void *obj)
{
    currentMode++;
    if (currentMode >= MODE_LENGTH)
    {
        currentMode = 0;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_OTA_MIN + has_ota + currentMode,
        NULL);
    ESP_LOGI(LOG_TAG, "Switching to App %d", currentMode);
    if (partition != NULL)
    {
        esp_err_t err = esp_ota_set_boot_partition(partition);
        if (err != ESP_OK)
        {
            ESP_LOGE(LOG_TAG, "%s: esp_ota_set_boot_partition failed (%s).", __func__, esp_err_to_name(err));
            abort();
            return;
        }
        esp_restart();
    }
    else
    {
        ESP_LOGE(LOG_TAG, "No partition found for App %d.", currentMode);
    }
}

void switch_ota_task(void *obj)
{
    if (!has_ota)
    {
        ESP_LOGE(LOG_TAG, "This project doesn't have OTA");
        vTaskDelete(NULL);
        return;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_OTA_0,
        NULL);
    ESP_LOGI(LOG_TAG, "Switching to OTA");
    if (partition != NULL)
    {
        esp_err_t err = esp_ota_set_boot_partition(partition);
        if (err != ESP_OK)
        {
            ESP_LOGE(LOG_TAG, "%s: esp_ota_set_boot_partition failed (%s).", __func__, esp_err_to_name(err));
            abort();
            return;
        }
        esp_restart();
    }
    else
    {
        ESP_LOGE(LOG_TAG, "No partition found for OTA.");
    }
}