#include "i2s.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h" // Sử dụng esp_log.h thay vì esp_check.h để sử dụng hàm log
#include "sdkconfig.h"

#include "marcro.h"
#include "config.h"
#include "PWM.h"
#include "logging.h"
#define DMA_BUFFER_LEN 4092

static i2s_chan_handle_t tx_chan; // I2S tx channel handler
static const char *TAG = "I2S";	  // Thẻ (tag) cho log
uint32_t i2s_port_data = 0;
static uint32_t *w_buf = (uint32_t *)calloc(DMA_BUFFER_LEN / sizeof(uint32_t), sizeof(uint32_t));
size_t bytes_written = 0;
void i2s_write(uint8_t pin, uint8_t val)
{
	SET_BIT_TO(i2s_port_data, pin, val);
}
uint8_t i2s_state(uint8_t pin)
{
	return TEST(i2s_port_data, pin);
}

void inline i2s_push_sample()
{
	for (;;)
	{
		size_t processed_samples = 0; // Đếm số lượng dữ liệu đã xử lý

		while ( processed_samples < DMA_BUFFER_LEN / 4)
		{
			for (uint8_t p = 0; p < MAX_EX_PIN; ++p)
			{
				if (pwm.pwm_pin_data[p].pwm_duty_ticks > 0)
				{ // pin has active pwm?
					if (pwm.pwm_pin_data[p].pwm_tick_count == 0)
					{
						if (TEST32(i2s_port_data, p))
						{ // hi->lo
							CBI32(i2s_port_data, p);
							pwm.pwm_pin_data[p].pwm_tick_count = pwm.pwm_pin_data[p].pwm_cycle_ticks - pwm.pwm_pin_data[p].pwm_duty_ticks;
						}
						else
						{ // lo->hi
							SBI32(i2s_port_data, p);
							pwm.pwm_pin_data[p].pwm_tick_count = pwm.pwm_pin_data[p].pwm_duty_ticks;
						}
					}
					else
						pwm.pwm_pin_data[p].pwm_tick_count--;
				}
			}

			// Sao chép dữ liệu từ i2s_port_data vào w_buf
			w_buf[processed_samples] = i2s_port_data; // Lưu từng mẫu dữ liệu vào buffer w_buf
			processed_samples++;					  // Tăng số mẫu đã xử lý
		}
		i2s_channel_write(tx_chan, w_buf, DMA_BUFFER_LEN, &bytes_written, 1000);
	}
}
static IRAM_ATTR bool i2s_tx_queue_overflow_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
	ESP_LOGW(TAG, "OVERFLOW Detected");
	return false; // Không yêu cầu xử lý thêm sau khi callback được gọi
}
static IRAM_ATTR bool i2s_tx_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
	// i2s_push_sample();
	// ESP_LOGW(TAG, "OVERFLOW Detected");
	return false; // Không yêu cầu xử lý thêm sau khi callback được gọi
}

void load_buf(void *parameter)
{
	i2s_push_sample();
}
#define I2S_CHANNEL_CONFIG(i2s_num, i2s_role) { \
	.id = i2s_num,                              \
	.role = i2s_role,                           \
	.dma_desc_num = 8,                          \
	.dma_frame_num = DMA_BUFFER_LEN,            \
	.auto_clear = true,                         \
}

bool i2s_init(void)
{
	// Cấu hình kênh truyền I2S
	i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

	// Tạo kênh I2S truyền (TX)
	esp_err_t err = i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL);
	if (err == ESP_ERR_NOT_FOUND)
	{
		ESP_LOGE(TAG, "Không tìm thấy kênh I2S khả dụng");
		return 0;
	}
	ESP_ERROR_CHECK(err);

#define I2S_CLK_CONFIG(rate) {              \
	.sample_rate_hz = rate,                 \
	.clk_src = I2S_CLK_SRC_PLL_160M,        \
	.mclk_multiple = I2S_MCLK_MULTIPLE_128, \
}
	i2s_std_config_t tx_std_cfg = {
		.clk_cfg = I2S_CLK_CONFIG(64000),
		.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
		.gpio_cfg = {
			.mclk = I2S_GPIO_UNUSED,
			.bclk = gpio_num_t(GPIO_PIN_I2S_SCL),
			.ws = gpio_num_t(GPIO_PIN_I2S_LATCH),
			.dout = gpio_num_t(GPIO_PIN_I2S_SDA),
			.din = I2S_GPIO_UNUSED,
			.invert_flags = {
				.mclk_inv = false,
				.bclk_inv = false,
				.ws_inv = false,
			},
		},
	};
	ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));

	// In thông tin về các chân I2S đã được cấu hình
	ESP_LOGI(TAG, "Cấu hình chân I2S:");
	ESP_LOGI(TAG, "MCLK Pin: %s", tx_std_cfg.gpio_cfg.mclk == I2S_GPIO_UNUSED ? "Không sử dụng" : "Được sử dụng");
	ESP_LOGI(TAG, "BCLK Pin: GPIO %d", tx_std_cfg.gpio_cfg.bclk);
	ESP_LOGI(TAG, "WS Pin: GPIO %d", tx_std_cfg.gpio_cfg.ws);
	ESP_LOGI(TAG, "DOUT Pin: GPIO %d", tx_std_cfg.gpio_cfg.dout);

	// Đăng ký callback sự kiện cho kênh truyền (TX)
	i2s_event_callbacks_t cbs = {
		.on_recv = NULL,
		.on_recv_q_ovf = NULL,
		.on_sent = i2s_tx_sent_callback,
		.on_send_q_ovf = i2s_tx_queue_overflow_callback,
	};
	ESP_ERROR_CHECK(i2s_channel_register_event_callback(tx_chan, &cbs, NULL));

	// Tạo task nạp dữ liệu vào bộ đệm
	BaseType_t task_created = xTaskCreatePinnedToCore(load_buf, "Load_buf", 10000, nullptr, 1, nullptr, 1);
	if (task_created != pdPASS)
	{
		ESP_LOGE(TAG, "Không thể tạo task 'Load_buf'");
		return 0;
	}

	// Kích hoạt kênh I2S
	//	vTaskDelay(pdMS_TO_TICKS(1000));
	ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

	DBGLN("\ni2s_init thành công");
	return 1;
}
