#include "blucontrol_mode.h"

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

void switch_mode_task();

int currentMode = -1;

void blucontrol_mode_init(void)
{
    const esp_partition_t* currentPartition = esp_ota_get_boot_partition();
    currentMode = currentPartition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN;
    for (int i = 0; i < MODE_LENGTH; i++)
    {
        if (mode_leds[i] < 0)
        {
            continue;
        }
        gpio_reset_pin(mode_leds[i]);
        gpio_set_direction(mode_leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(mode_leds[i], i == currentMode);
    }
}

uint32_t last_tick = 0;
void blucontrol_handle_buttons(void)
{
    bool all_pressed = true;
    for (int i = 0; i < SWITCH_BUTTONS_LENGTH; i++)
    {
        if (switch_buttons[i] < 0)
        {
            if (!ignored_switch_buttons[i])
            {
                printf("BluControl WARN: Switch Button %d has a non-positive value. Ignoring this button.\n", i + 1);
                ignored_switch_buttons[i] = true;
            }
            if (SWITCH_BUTTONS_LENGTH == 1)
            {
                all_pressed = false;
            }
            continue;
        }

        if (gpio_get_level(switch_buttons[i]))
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
            blucontrol_switch_mode();
        }
    }
    else
    {
        last_tick = 0;
    }
}

TaskHandle_t switchModeTaskHandle = NULL;
void blucontrol_switch_mode(void)
{
    xTaskCreatePinnedToCore(switch_mode_task, "BLU_SWITCH_MODE", 4096, NULL, tskIDLE_PRIORITY, &switchModeTaskHandle, 1);
    configASSERT(switchModeTaskHandle);
}

void switch_mode_task(void)
{
    currentMode++;
    if (currentMode >= MODE_LENGTH)
    {
        currentMode = 0;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_OTA_MIN + currentMode,
        NULL);
    printf("Switching to OTA %d\n", ESP_PARTITION_SUBTYPE_APP_OTA_MIN + currentMode);
    if (partition != NULL)
    {
        esp_err_t err = esp_ota_set_boot_partition(partition);
        if (err != ESP_OK)
        {
            printf("%s: esp_ota_set_boot_partition failed (%d).\n", __func__, err);
            abort();
            return;
        }
        esp_restart();
    }
    else
    {
        printf("%s: No partition found for OTA %d.\n", __func__, currentMode);
    }
}