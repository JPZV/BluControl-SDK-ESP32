idf_component_register(SRCS 
                    "blucontrol_mode.c"
                    INCLUDE_DIRS "include"
                    REQUIRES app_update driver
                    esp_partition esp_system)
