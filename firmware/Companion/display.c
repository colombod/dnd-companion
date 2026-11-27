#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "lvgl.h"
#include "panel_table.inc"

static lv_disp_drv_t driver;
static lv_disp_draw_buf_t buffers;
static esp_lcd_panel_handle_t panel;
static volatile unsigned frames;
static lv_color_t *capture;

static bool flush_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *ctx) {
    frames++;
    lv_disp_flush_ready(&driver);
    return false;
}

unsigned display_transfers(void) { return frames; }

static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *pixels) {
    if (capture) {
        size_t width = a->x2-a->x1+1;
        for (int y=a->y1; y<=a->y2; ++y)
            memcpy(capture+y*360+a->x1, pixels+(y-a->y1)*width, width*sizeof(*pixels));
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, pixels));
}

void display_capture(lv_color_t *pixels) {
    capture = pixels;
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    capture = NULL;
}

static void round_area(lv_disp_drv_t *d, lv_area_t *a) {
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
}

void display_init(void) {
    const size_t pixels = 360 * 24;
    spi_bus_config_t bus = SH8601_PANEL_BUS_QSPI_CONFIG(13, 15, 16, 17, 18, pixels * 2);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t cfg = SH8601_PANEL_IO_QSPI_CONFIG(14, flush_done, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &cfg, &io));
    sh8601_vendor_config_t vendor = {.init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags.use_qspi_interface = 1};
    esp_lcd_panel_dev_config_t pcfg = {.reset_gpio_num = 21, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16, .vendor_config = &vendor};
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &pcfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    lv_init();
    lv_color_t *a = heap_caps_malloc(pixels * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_color_t *b = heap_caps_malloc(pixels * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(a && b ? ESP_OK : ESP_ERR_NO_MEM);
    lv_disp_draw_buf_init(&buffers, a, b, pixels);
    lv_disp_drv_init(&driver);
    driver.hor_res = driver.ver_res = 360;
    driver.draw_buf = &buffers;
    driver.flush_cb = flush;
    driver.rounder_cb = round_area;
    lv_disp_drv_register(&driver);
}
