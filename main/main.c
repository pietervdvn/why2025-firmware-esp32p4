
// SPDX-License-Identifier: MIT

#include "bsp.h"
#include "bsp/why2025_coproc.h"
#include "bsp_device.h"
#include "hardware/why2025.h"
#include "pax_gfx.h"
#include "pax_gui.h"

#include <stdio.h>

#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include "bsp_keymap.h"

bsp_input_devtree_t const input_tree = {
    .common = {
        .type = BSP_EP_INPUT_WHY2025_CH32,
    },
    .category           = BSP_INPUT_CAT_KEYBOARD,
    .keymap             = &bsp_keymap_why2025,
    .backlight_endpoint = 0,
    .backlight_index    = 1,
};
bsp_input_devtree_t const input2_tree = {
    .common   = {
        .type = BSP_EP_INPUT_GPIO,
    },
    .category = BSP_INPUT_CAT_GENERIC,
    .pinmap   = &(bsp_pinmap_t const) {
        .pins_len  = 1,
        .pins      = (uint8_t const[]) {35},
        .activelow = false,
    },
};
bsp_led_devtree_t const led_tree = {
    .common   = {
        .type = BSP_EP_LED_WHY2025_CH32,
    },
    .num_leds = 2,
    .ledfmt   = {
        .color = BSP_PIXFMT_16_GREY,
    }
};
bsp_display_devtree_t const disp_tree = {
    .common = {
        .type      = BSP_EP_DISP_ST7701,
        .reset_pin = 0,
    },
    .pixfmt = {BSP_PIXFMT_16_565RGB, false},
    .h_fp   = BSP_DSI_LCD_HFP,
    .width  = BSP_DSI_LCD_H_RES,
    .h_bp   = BSP_DSI_LCD_HBP,
    .h_sync = BSP_DSI_LCD_HSYNC,
    .v_fp   = BSP_DSI_LCD_VFP,
    .height = BSP_DSI_LCD_V_RES,
    .v_bp   = BSP_DSI_LCD_VBP,
    .v_sync = BSP_DSI_LCD_VSYNC,
    .backlight_endpoint = 0,
    .backlight_index    = 0,
};
bsp_devtree_t const tree = {
    .input_count = 2,
    .input_dev = (bsp_input_devtree_t const *const[]) {
        &input_tree,
        &input2_tree,
    },
    .led_count = 1,
    .led_dev = (bsp_led_devtree_t const *const[]){
        &led_tree,
    },
    .disp_count = 1,
    .disp_dev   = (bsp_display_devtree_t const *const[]) {
        &disp_tree,
    },
};

pgui_grid_t gui = PGUI_NEW_GRID(
    10,
    10,
    216,
    100,
    2,
    3,

    &PGUI_NEW_LABEL("Row 1"),
    &PGUI_NEW_TEXTBOX(),

    &PGUI_NEW_LABEL("Row 2"),
    &PGUI_NEW_BUTTON("Hello,"),

    &PGUI_NEW_LABEL("Row 2"),
    &PGUI_NEW_BUTTON("World!")
);



int block_rows = 3;
int block_columns = 8;
int block_width_offset = 4;
int block_height_offset = 4;
int paddle_width = 64;


/*
* Checks if a block is hit; returns '1' if this is the case.
* This block will be marked as 'dead' and rendered black
*/
int hitBlocks(int x, int y, int* blocks, pax_buf_t* gfx ) {
    int block_width = ((BSP_DSI_LCD_V_RES - 40) / block_columns) - block_width_offset;
    int block_height = ((BSP_DSI_LCD_H_RES - 40) / block_columns) - block_height_offset;


    if(x < 20 || y < 20){
        return 0;
    }
   // first index
    int i = (y - 20) / (block_height + block_height_offset);
    // second index
    int j = (x - 20) / ( block_width + block_width_offset);
    if(i >= block_rows || i < 0) {
        return 0;
    }
    if(j >= block_columns || j < 0) {
        return 0;
    }


    if(blocks[i * block_columns + j] == 1 ){
        // Block is still alive

        blocks[i * block_columns + j] = 0;
            pax_draw_rect(gfx, 0xff000000,
                        20 + j * (block_width + block_width_offset),
                        20 + i * (block_height + block_height_offset) , block_width, block_height);

        int blockLower = 20 + (i + 1) * (block_height + block_height_offset);
        int blockUpper = 20 + (i) * (block_height + block_height_offset);

        if( blockLower - 12 > y && y > blockUpper + 12) {
            return 2; // We've hit the side
        }

        return 1;
    }
    return 0;

}

pax_buf_t gfx;
void app_main(void) {

   // Initialize the framebuffer to write to the display
    pax_buf_init(&gfx, NULL, BSP_DSI_LCD_H_RES, BSP_DSI_LCD_V_RES, PAX_BUF_16_565RGB);
    // Set in landscape mode (clockwise rotation)
    pax_buf_set_orientation(&gfx, PAX_O_ROT_CW);

    // Set endian-ness
    pax_buf_reversed(&gfx, false);


    bsp_init();
    uint32_t dev_id = bsp_dev_register(&tree);

    // Turn on the backlight of the display
    bsp_disp_backlight(dev_id, 0, 65535);

    // Set background color. There are four channels, Alpha, Red, Green, Blue
    pax_background(&gfx, pgui_theme_default.bg_col);

    // Calculate the layout of the GUI, which can draw some components as buttons, tables, ...
    //  pgui_calc_layout(pax_buf_get_dims(&gfx), (pgui_elem_t *)&gui, NULL);
    // ... and draw it to the buffer
    // pgui_draw(&gfx, (pgui_elem_t *)&gui, NULL);
    // Finally, send the framebuffer to the screen
    bsp_disp_update(dev_id, 0, pax_buf_get_pixels(&gfx));


    int width = pax_buf_get_width(&gfx);
    int height = pax_buf_get_height(&gfx);

    int block_width = ((BSP_DSI_LCD_V_RES - 40) / block_columns) - block_width_offset;
    int block_height = ((BSP_DSI_LCD_H_RES - 40) / block_columns) - block_height_offset;


    // the puck
    int x = width / 2;
    int y = height / 2;
    int x_speed = 0;
    int y_speed = 4;

    int paddle_x = width / 2;
    int paddle_speed = 0;

    int blocks[block_rows][block_columns];
    pax_background(&gfx, 0x00000000);

    int alive_blocks = block_columns * block_rows;

    int i, j;
    for (i = 0; i < block_rows; i++) {
      for (j = 0; j < block_columns; j++) {
        blocks[i][j] = 1;
        pax_draw_rect(&gfx, 0xff0000ff + i * 256 * 100,
        20 + j * (block_width + block_width_offset),
        20 + i * (block_height + block_height_offset) , block_width, block_height);
      }
    }

    // esp_timer_get_time is the current time, in microseconds
    int64_t start = esp_timer_get_time() / 1000;

    int lives = 2;
    int colors_trail[3] = {0xff220000,0xff002200,0xff000033};
    int colors[3] = {0xffdd2222,0xff22cc22,0xff2222cc};

    while (lives >= 0) {

        vTaskDelay(2.5); // in ticks; one tick is one millis
        int64_t d = (esp_timer_get_time() / 1000) - start;

        // draw a black rect at the old position of the paddle
        pax_draw_rect(&gfx, 0xff000000, paddle_x - paddle_width / 2, height - 40, paddle_width, 12); // x, y, w, h
        pax_draw_circle(&gfx, colors_trail[2 - lives], x, y, 12);

        paddle_x += paddle_speed;

        if(paddle_x - paddle_width / 2 < 0) {
            paddle_x = paddle_width / 2;
        }else if (paddle_x + paddle_width / 2 >= width) {
            paddle_x = width - paddle_width / 2;
        }

        if(y >= height - 40 && x > paddle_x - paddle_width / 2 && x < paddle_x + paddle_width / 2) {
        // Paddle is hit
            if(y_speed > 0){
                y_speed = -y_speed;
                y += y_speed;
                // How central are we?
               // int delta = ; // between -24 and 24
                x_speed = (16 * (x - paddle_x)) / paddle_width;

            }
        }

        int didHit = hitBlocks(x, y, &blocks, &gfx);
        if(didHit == 1){
             y_speed = -y_speed;
             alive_blocks --;
        }else if (didHit == 2) {
            x_speed = -x_speed;
            alive_blocks --;
        }

        x += x_speed;
        y += y_speed;

        if(x <= 0){
            x_speed = -x_speed;
            x = x + x_speed;
        }
        if(x >= width) {
            x_speed = -x_speed;
            x = x+x_speed;
        }

        if(y <= 0){
            y_speed = -y_speed;
            y = y + y_speed;
        }
        if(y >= height) {
            y_speed = 4;
            x_speed = 0;
            lives --;
            vTaskDelay(250);
            x = width / 2;
            y = height / 2;
            paddle_x = width / 2;
        }




        bsp_event_t event;
        // Wait for an event. The timeout is the second argument is the timeout.
        if (bsp_event_wait(&event, 0)) {
            //  Translate a hardware event into a GUI event
            /* pgui_event_t p_event = {
                     .type    = event.input.type,
                     .input   = event.input.nav_input,
                     .value   = event.input.text_input,
                     .modkeys = event.input.modkeys,
            }; */

            if(event.input.type == BSP_INPUT_EVENT_PRESS){
                if(event.input.nav_input == BSP_INPUT_LEFT) {
                    paddle_speed = -6;
                }else if (event.input.nav_input == BSP_INPUT_RIGHT){
                    paddle_speed = 6;
                }
            } else {
                paddle_speed = 0;
            }


            // and send it to the UI. This will give back a response
            // If it is null, the event is ignored
            // If not null, it
            /*pgui_resp_t resp = pgui_event(pax_buf_get_dims(&gfx), (pgui_elem_t *)&gui, NULL, p_event);
            if (resp) {
                pgui_redraw(&gfx, (pgui_elem_t *)&gui, NULL);
                bsp_disp_update(dev_id, 0, pax_buf_get_pixels(&gfx));
            }*/
        }


        {
            // Draw everything

            if(alive_blocks <= 0){
                char *my_text    = "You WIN!";
                pax_draw_text(&gfx, 0xffffffff, pax_font_sky_mono, 54, width / 2 - 8 * 40, height / 2, my_text);
            }

           if(lives < 0){
                char *my_text    = "You LOSE!";
                pax_draw_text(&gfx, 0xffffffff, pax_font_sky_mono, 54, width / 2 - 8 * 40, height / 2, my_text);
            }else {
                pax_draw_circle(&gfx, colors[2 - lives], x, y, 12);
                pax_draw_rect(&gfx, 0xffffffff, paddle_x - paddle_width / 2, height - 40, paddle_width, 12); // x, y, w, h
            }

            bsp_disp_update(dev_id, 0, pax_buf_get_pixels(&gfx));
            if(alive_blocks <= 0){
                break;
            }
        }
    }
}
