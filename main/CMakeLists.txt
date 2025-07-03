idf_component_register(SRCS "main.c"
                            "framebuffer.c"
                            "canvas.c"
                            "app_manager.c"
                            "fonts.c"
                            "image_utils.c"
                            "animations.c"
                            "http_client.c"
                            "mqtt.c"
                    INCLUDE_DIRS "." "include" "../external/qoi")

set(image ../fatfs_image)
fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)
