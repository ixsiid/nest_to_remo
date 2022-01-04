#include <esp_log.h>
#include <driver/adc.h>
#include <wifiManager.hpp>

#include "http_event_handler.h"

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>

#include <button.hpp>

#include "wifi_credential.h"
/*
#define SSID "****************"
#define PASSWORD "****************"
*/
#include "remo_credential.h"
/*
#define BEARER_TOKEN "***************************************************************************************"
#define APPLIANCE_ID "************************************"
*/

#define TAG "NEST TO REMO"

extern "C" {
void app_main();
}

static LGFX gfx;

#define HEAT_PIN GPIO_NUM_35
#define COOL_PIN GPIO_NUM_36
#define HEAT_CH ADC1_CHANNEL_7
#define COOL_CH ADC1_CHANNEL_0

#pragma pack(1)
typedef union {
	int data;
	struct {
		bool warm;
		bool cool;
		bool enable;
		bool manual_stop;
	};
} status_u;
#pragma pack()

void send(status_u);

status_u current_status;

void app_main() {
	ESP_LOGI(TAG, "Start");
	WiFi::Connect(SSID, PASSWORD, 3000);
	esp_ip4_addr_t *ip = WiFi::getIp();
	ESP_LOGI(TAG, "IP: %s", ip ? inet_ntoa(*WiFi::getIp()) : "None");

	gfx.init();
	gfx.println("Hello, world!!");
	char text[32] = {0};
	snprintf(text, sizeof(text), "IP: %s", ip ? inet_ntoa(*WiFi::getIp()) : "None");
	gfx.println(text);

	gpio_set_direction(HEAT_PIN, gpio_mode_t::GPIO_MODE_INPUT);
	gpio_set_direction(COOL_PIN, gpio_mode_t::GPIO_MODE_INPUT);

	adc1_config_channel_atten(adc1_channel_t::HEAT_CH, adc_atten_t::ADC_ATTEN_DB_11);  // G35
	adc1_config_channel_atten(adc1_channel_t::COOL_CH, adc_atten_t::ADC_ATTEN_DB_11);  // G36

	adc_power_acquire();

	int heat, cool;
	int blink = 0;

	const int display_width	= 320;
	const int graph_width	= 150;
	const int display_center = display_width / 2;

	gfx.drawRect(display_center - graph_width - 1, 63, graph_width + 2, 66, TFT_SKYBLUE);
	gfx.drawRect(display_center + 1, 63, graph_width + 2, 66, TFT_BROWN);
	gfx.fillRect(display_center, 63, 2, 66, TFT_WHITE);

	current_status.warm		  = false;
	current_status.cool		  = false;
	current_status.enable	  = true;
	current_status.manual_stop = false;
	status_u st;

	uint8_t buttonPins[] = {37, 38, 39};
	Button *button		 = new Button(buttonPins, 3);

	while (true) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);

		gfx.setColor(TFT_WHITE);

		heat = adc1_get_raw(adc1_channel_t::HEAT_CH);
		cool = adc1_get_raw(adc1_channel_t::COOL_CH);

		gfx.setCursor(12, 24);
		snprintf(text, sizeof(text), "Heat: %f V", heat * 2.45f / 4095.0f);
		gfx.println(text);
		gfx.setCursor(12, 40);
		snprintf(text, sizeof(text), "Cool: %f V", cool * 2.45f / 4095.0f);
		gfx.println(text);

		int heat_w = heat * graph_width / 4095;
		int cool_w = cool * graph_width / 4095;
		gfx.fillRect(display_center - graph_width, 64, graph_width, 64, TFT_BLACK);
		gfx.fillRect(display_center - cool_w, 64, cool_w, 64, TFT_BLUE);
		gfx.fillRect(display_center + 2, 64, graph_width, 64, TFT_BLACK);
		gfx.fillRect(display_center + 2, 64, heat_w, 64, TFT_ORANGE);

		st.warm		 = heat > 1500;
		st.cool		 = cool > 1500;
		bool force_send = false;
		button->check(nullptr, [&](uint8_t pin) {
			switch (pin) {
				case 39:
					force_send = true;
					break;
				case 38:
					st.enable	 = !(current_status.enable);
					force_send = st.enable;
					if (st.enable) st.manual_stop = false;
					break;
				case 37:
					st.manual_stop = true;
					st.enable		= false;
					force_send	= true;
					break;
			}
		});

		if (force_send ||
		    (st.enable && (current_status.data != st.data))) {
			printf("status: warm %d, cool %d, enable %d, stop %d\n", st.warm, st.cool, st.enable, st.manual_stop);
			send(st);
		}
		current_status.data = st.data;

		gfx.setColor(TFT_WHITE);
		gfx.setCursor(30, 200);
		gfx.print("Resend");
		gfx.setCursor(120, 200);
		gfx.print(current_status.enable ? "Enable" : "Disable");
		gfx.setCursor(220, 200);
		gfx.print("Stop");

		gfx.setColor(blink ? TFT_YELLOW : TFT_GREENYELLOW);
		gfx.fillRect(5, 230, 5, 5);
		blink = 1 - blink;
	}
	/**/
}

const char root_cert_pem_start[] =
    "-----BEGIN CERTIFICATE-----\n\
MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV\n\
BAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIw\n\
MAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0\n\
eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJV\n\
UzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UE\n\
ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZp\n\
ZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n\
MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/\n\
y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0N\n\
Tm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRo\n\
Ot+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0C\n\
zyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg5J\n\
Q4W2bHO7ZOphQazJ1FTfhy/HIrImzJ9ZVGif/L4qL8RVHHVAYBeFAlU5i38FAgMB\n\
AAGjgfAwge0wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0O\n\
BBYEFJxfAN+qAdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFL9ft9HO3R+G9FtV\n\
rNzXEMIOqYjnME8GCCsGAQUFBwEBBEMwQTAcBggrBgEFBQcwAYYQaHR0cDovL28u\n\
c3MyLnVzLzAhBggrBgEFBQcwAoYVaHR0cDovL3guc3MyLnVzL3guY2VyMCYGA1Ud\n\
HwQfMB0wG6AZoBeGFWh0dHA6Ly9zLnNzMi51cy9yLmNybDARBgNVHSAECjAIMAYG\n\
BFUdIAAwDQYJKoZIhvcNAQELBQADggEBACMd44pXyn3pF3lM8R5V/cxTbj5HD9/G\n\
VfKyBDbtgB9TxF00KGu+x1X8Z+rLP3+QsjPNG1gQggL4+C/1E2DUBc7xgQjB3ad1\n\
l08YuW3e95ORCLp+QCztweq7dp4zBncdDQh/U90bZKuCJ/Fp1U1ervShw3WnWEQt\n\
8jxwmKy6abaVd38PMV4s/KCHOkdp8Hlf9BRUpJVeEXgSYCfOn8J3/yNTd126/+pZ\n\
59vPr5KW7ySaNRB6nJHGDn2Z9j8Z3/VyVOEVqQdZe4O/Ui5GjLIAZHYcSNPYeehu\n\
VsyuLAOQ1xk4meTKCRlb/weWsKh/NEnfVqn3sF/tM+2MR7cwA130A4w=\n\
-----END CERTIFICATE-----";

static void http_post_task(const char *payload) {
	esp_http_client_config_t config = {0};
	config.host				  = "api.nature.global";
	config.path				  = "/1/appliances/" APPLIANCE_ID "/aircon_settings";
	config.transport_type		  = HTTP_TRANSPORT_OVER_SSL;
	config.event_handler		  = _http_event_handler;
	config.cert_pem			  = root_cert_pem_start;
	esp_http_client_handle_t client = esp_http_client_init(&config);

	esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_header(client, "Authorization", "Bearer " BEARER_TOKEN);
	esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
	esp_http_client_set_post_field(client, payload, strlen(payload));

	esp_err_t err;
	while (1) {
		err = esp_http_client_perform(client);
		if (err != ESP_ERR_HTTP_EAGAIN) break;
	}

	if (err == ESP_OK) {
		ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
			    esp_http_client_get_status_code(client),
			    esp_http_client_get_content_length(client));
	} else {
		ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
	}
	esp_http_client_cleanup(client);
}

enum mode_e {
	Empty,
	Cool,
	Warm,
	Dry,
	Blow,
	Auto,
};
const char mode_str[] = "\0\0\0\0\0cool\0warm\0dry\0\0blow\0auto";
const char vol_str[] =
    "\0\0\0\0\0auto\0"
    "1\0\0\0\0"
    "2\0\0\0\0"
    "3\0\0\0\0"
    "4\0\0\0\0"
    "5\0\0\0\0"
    "6\0\0\0\0"
    "7\0\0\0\0"
    "8\0\0\0\0"
    "9\0\0\0\0"
    "10\0\0";
const char dir_str[] =
    "\0\0\0\0\0auto\0"
    "1\0\0\0\0"
    "2\0\0\0\0"
    "3\0\0\0\0"
    "4\0\0\0\0"
    "5\0\0\0";

void set(int temperature, mode_e mode, int volume, int direction, bool power) {
	if (volume < -1 || volume > 10) volume = -1;
	if (direction < -1 || direction > 5) direction = -1;

	if (mode == mode_e::Warm) {
		if (temperature < 14 * 2) temperature = 14 * 2;
		if (temperature > 30 * 2) temperature = 30 * 2;
	} else if (mode == mode_e::Cool) {
		if (temperature < 18 * 2) temperature = 18 * 2;
		if (temperature > 32 * 2) temperature = 32 * 2;
	} else {
		temperature = 0;
	}

	char payload[128];
	int n;
	if (temperature) {
		n = snprintf(payload, sizeof(payload),
				   "temperature=%d%s", temperature / 2, temperature & 0x1 ? ".5" : "");
	} else {
		n = snprintf(payload, sizeof(payload), "temperature=");
	}
	n = n + snprintf(payload + n, sizeof(payload) - n,
				  "&operation_mode=%s&air_volume=%s&air_direction=%s&button=%s",
				  mode_str + 5 * mode,
				  vol_str + 5 * (volume + 1),
				  dir_str + 5 * (direction + 1),
				  power ? "" : "power-off");

	printf("length: %d\n%s\n", n, payload);
	if(WiFi::isConnected()) http_post_task(payload);
}

void send(status_u status) {
	if (status.warm && status.cool) {
		ESP_LOGE(TAG, "Invalid status");
		return;
	}
	if (status.manual_stop || !(status.warm || status.cool)) {
		set(0, mode_e::Empty, -1, -1, false);
		return;
	}
	if (status.warm) {
		set(24 * 2, mode_e::Warm, -1, -1, true);
		return;
	}
	if (status.cool) {
		set(20 * 2, mode_e::Cool, -1, -1, true);
		return;
	}
	printf("Ko ko ma de ki ta ra    o ka shii ii yooooo: %d", status.data);
}
