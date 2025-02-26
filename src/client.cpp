/**
 * ZeDMD command line client
 *
 * --help
 * --version
 * --port                         Serial port to connect. If omitted, auto-discovery will be used.
 * --info                         Show information about ZeDMD.
 * --ip-address                   IP address to connect. If omitted, auto-discovery will be used.
 * --set-brightness               0..15
 * --set-debug                    0,1
 * --set-panel-clkphase           0,1
 * --set-panel-driver             0(SHIFTREG),1(FM6124),2(FM6126A),3(ICN2038S),4(MBI5124),5(SM5266P),6(DP3246_SM5368)
 * --set-panel-i2sspeed           8,16,20
 * --set-panel-latch-blanking     0,1,2,3,4
 * --set-panel-min-refresh-rate   30..120
 * --set-rgb-order                0..5
 * --set-transport                0(USB),1(UDP),2(TCP),3(SPI)
 * --set-udp-delay                0..9
 * --set-usb-package-size         32..1920 (step 32)
 * --set-wifi-ssid
 * --set-wifi-password
 * --set-wifi-port
 * --set-y-offset                 0..32
 *
 * @todo
 * --download-config
 * --upload-config
 * --upload-screensaver
 */

#include <inttypes.h>

#include <string>
#include <thread>

#include "ZeDMD.h"
#include "cargs.h"
#include "libserialport.h"

static struct cag_option options[] = {
    {.identifier = 'h', .access_letters = "h", .access_name = "help", .description = "Show zedmd-client help"},
    {.identifier = 'v', .access_letters = "v", .access_name = "version", .description = "Show zedmd-client version"},
    {.identifier = '5', .access_name = "verbose", .description = "Verbose log messages"},
    {.identifier = 'p',
     .access_letters = "p",
     .access_name = "port",
     .value_name = "VALUE",
     .description = "Serial port to connect. If omitted, auto-discovery will be used."},
    {.identifier = 'i',
     .access_letters = "i",
     .access_name = "info",
     .value_name = NULL,
     .description = "Show information about ZeDMD."},
    {.identifier = 'a',
     .access_letters = "a",
     .access_name = "ip-address",
     .value_name = "VALUE",
     .description = "IP address to connect. If omitted, auto-discovery will be used."},
    {.identifier = 'b',
     .access_letters = "b",
     .access_name = "set-brightness",
     .value_name = "VALUE",
     .description = "0..15"},
    {.identifier = 'd', .access_letters = "d", .access_name = "set-debug", .value_name = "VALUE", .description = "0,1"},
    {.identifier = '0', .access_name = "set-panel-clkphase", .value_name = "VALUE", .description = "0,1"},
    {.identifier = '1',
     .access_name = "set-panel-driver",
     .value_name = "VALUE",
     .description = "0(SHIFTREG), 1(FM6124), 2(FM6126A), 3(ICN2038S), 4(MBI5124), 5(SM5266P), 6(DP3246_SM5368)"},
    {.identifier = '2', .access_name = "set-panel-i2sspeed", .value_name = "VALUE", .description = "8,16,20"},
    {.identifier = '3', .access_name = "set-panel-latch-blanking", .value_name = "VALUE", .description = "0,1,2,3,4"},
    {.identifier = '4', .access_name = "set-panel-min-refresh-rate", .value_name = "VALUE", .description = "30..120"},
    {.identifier = 'o',
     .access_letters = "o",
     .access_name = "set-rgb-order",
     .value_name = "VALUE",
     .description = "0..5"},
    {.identifier = 't',
     .access_letters = "t",
     .access_name = "set-transport",
     .value_name = "VALUE",
     .description = "0(USB), 1(WiFi UDP), 2(WiFi TCP), 3(SPI)"},
    {.identifier = 'u',
     .access_letters = "u",
     .access_name = "set-udp-delay",
     .value_name = "VALUE",
     .description = "0..9"},
    {.identifier = 's',
     .access_letters = "s",
     .access_name = "set-usb-package-size",
     .value_name = "VALUE",
     .description = "32..1920 (step 32)"},
    {.identifier = '6', .access_name = "set-wifi-ssid", .value_name = "VALUE", .description = "WiFi network SSID"},
    {.identifier = '7',
     .access_name = "set-wifi-password",
     .value_name = "VALUE",
     .description = "WiFi network password"},
    {.identifier = '8',
     .access_name = "set-wifi-port",
     .value_name = "VALUE",
     .description = "WiFi network port (default is 3333)"},
    {.identifier = '9',
     .access_name = "set-wifi-power",
     .value_name = "VALUE",
     .description =
         "WiFi power, default is 80, supported values 84(21dBm), 82(20.5dBm), 80(20dBm), 78(19.5dBm), 76(19dBm), "
         "74(18.5dBm), 68(17dBm), 60(15dBm), 52(13dBm), 44(11dBm), 34(8.5dBm), 28(7dBm), 20(5dBm), 8(2dBm)"},
    {.identifier = 'l',
     .access_letters = "l",
     .access_name = "led-test",
     .value_name = "VALUE",
     .description = "run LED test"}};

void ZEDMDCALLBACK LogCallback(const char* format, va_list args, const void* pUserData)
{
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);

  printf("%s\n", buffer);
}

int main(int argc, char* argv[])
{
  char identifier;
  cag_option_context cag_context;
  bool wifi = false;

  const char* opt_port = NULL;
  const char* opt_ip_address = NULL;
  bool opt_info = false;
  bool opt_version = false;
  bool opt_verbose = false;
  const char* opt_brightness = NULL;
  const char* opt_debug = NULL;
  const char* opt_panel_clkphase = NULL;
  const char* opt_panel_i2sspeed = NULL;
  const char* opt_panel_latch_blanking = NULL;
  const char* opt_panel_min_refresh_rate = NULL;
  const char* opt_panel_driver = NULL;
  const char* opt_rgb_order = NULL;
  const char* opt_transport = NULL;
  const char* opt_udp_delay = NULL;
  const char* opt_usb_package_size = NULL;
  const char* opt_wifi_ssid = NULL;
  const char* opt_wifi_password = NULL;
  const char* opt_wifi_port = NULL;
  const char* opt_wifi_power = NULL;
  const char* opt_y_offset = NULL;
  bool opt_led_test = false;

  bool has_other_options_than_h = false;
  cag_option_init(&cag_context, options, CAG_ARRAY_SIZE(options), argc, argv);
  while (cag_option_fetch(&cag_context))
  {
    identifier = cag_option_get_identifier(&cag_context);
    switch (identifier)
    {
      case 'v':
        opt_version = true;
        has_other_options_than_h = true;
        break;
      case '5':
        opt_verbose = true;
        break;
      case 'p':
        opt_port = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 'i':
        opt_info = true;
        has_other_options_than_h = true;
        break;
      case 'a':
        opt_ip_address = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 'b':
        opt_brightness = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 'd':
        opt_debug = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '0':
        opt_panel_clkphase = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '1':
        opt_panel_driver = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '2':
        opt_panel_i2sspeed = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '3':
        opt_panel_latch_blanking = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '4':
        opt_panel_min_refresh_rate = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 'o':
        opt_rgb_order = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 't':
        opt_transport = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 'u':
        opt_udp_delay = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 's':
        opt_usb_package_size = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '6':
        opt_wifi_ssid = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '7':
        opt_wifi_password = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '8':
        opt_wifi_port = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case '9':
        opt_wifi_power = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 'y':
        opt_y_offset = cag_option_get_value(&cag_context);
        has_other_options_than_h = true;
        break;
      case 'l':
        opt_led_test = true;
        has_other_options_than_h = true;
        break;
    }
  }

  if (!has_other_options_than_h)
  {
    printf("Usage: zedmd-client [OPTION]...\n");
    cag_option_print(options, CAG_ARRAY_SIZE(options), stdout);
    printf("Warning: It is recommended to use an USB conection to change settings.\n");
    printf("It might work over WiFi, but especially UDP is not reliable.\n");
    printf("Use this command to switch to USB first:\n");
    printf("zedmd-client --set-transport=0\n");
    return 0;
  }

  uint8_t brightness;
  if (opt_brightness)
  {
    brightness = (uint8_t)std::stoi(std::string(opt_brightness));
    if (!(brightness >= 0 && brightness <= 15))
    {
      printf("Error: brightness has to be betwen 0 and 15.\n");
      return -1;
    }
  }

  uint8_t debug;
  if (opt_debug)
  {
    debug = (uint8_t)std::stoi(std::string(opt_debug));
    if (debug != 0 && debug != 1)
    {
      printf("Error: debug has to 0 or 1.\n");
      return -1;
    }
  }

  uint8_t panel_clkphase;
  if (opt_panel_clkphase)
  {
    panel_clkphase = (uint8_t)std::stoi(std::string(opt_panel_clkphase));
    if (panel_clkphase != 0 && panel_clkphase != 1)
    {
      printf("Error: panel-clkphase has to 0 or 1.\n");
      return -1;
    }
  }

  uint8_t panel_driver;
  if (opt_panel_driver)
  {
    panel_driver = (uint8_t)std::stoi(std::string(opt_panel_driver));
    if (!(panel_driver >= 0 && panel_driver <= 6))
    {
      printf("Error: panel-driver has to between 0 and 6.\n");
      return -1;
    }
  }

  uint8_t panel_i2sspeed;
  if (opt_panel_i2sspeed)
  {
    panel_i2sspeed = (uint8_t)std::stoi(std::string(opt_panel_i2sspeed));
    if (panel_i2sspeed != 8 && panel_i2sspeed != 16 && panel_i2sspeed != 20)
    {
      printf("Error: panel-i2sspeed has to be 8, 16 or 20.\n");
      return -1;
    }
  }

  uint8_t panel_latch_blanking;
  if (opt_panel_latch_blanking)
  {
    panel_latch_blanking = (uint8_t)std::stoi(std::string(opt_panel_latch_blanking));
    if (!(panel_latch_blanking >= 0 && panel_latch_blanking <= 4))
    {
      printf("Error: panel-latch-blanking has to be between 0 and 4.\n");
      return -1;
    }
  }

  uint8_t panel_min_refresh_rate;
  if (opt_panel_min_refresh_rate)
  {
    panel_min_refresh_rate = (uint8_t)std::stoi(std::string(opt_panel_min_refresh_rate));
    if (!(panel_min_refresh_rate >= 30 && panel_min_refresh_rate <= 120))
    {
      printf("Error: panel-min-refresh-rate has to between 30 and 120.\n");
      return -1;
    }
  }

  uint8_t rgb_order;
  if (opt_rgb_order)
  {
    rgb_order = (uint8_t)std::stoi(std::string(opt_rgb_order));
    if (!(rgb_order >= 0 && rgb_order <= 5))
    {
      printf("Error: rgb-order has to between 0 and 5.\n");
      return -1;
    }
  }

  uint8_t transport;
  if (opt_transport)
  {
    transport = (uint8_t)std::stoi(std::string(opt_transport));
    if (!(transport >= 0 && transport <= 3))
    {
      printf("Error: transport has to be betwen 0 and 15.\n");
      return -1;
    }
  }

  uint8_t udp_delay;
  if (opt_udp_delay)
  {
    udp_delay = (uint8_t)std::stoi(std::string(opt_udp_delay));
    if (!(udp_delay >= 0 && udp_delay <= 9))
    {
      printf("Error: udp-delay has to be betwen 0 and 9.\n");
      return -1;
    }
  }

  uint16_t usb_package_size;
  if (opt_usb_package_size)
  {
    usb_package_size = (uint16_t)std::stoi(std::string(opt_usb_package_size));
    if (!(usb_package_size >= 32 && usb_package_size <= 1920 && ((usb_package_size % 32) == 0)))
    {
      printf("Error: usb-package-size has to be betwen 32 and 1920, using steps of 32, so 32, 64, 96, 128, ...\n");
      return -1;
    }
  }

  uint16_t wifi_port;
  if (opt_wifi_port)
  {
    wifi_port = (uint16_t)std::stoi(std::string(opt_wifi_port));
    if (!(wifi_port >= 0 && wifi_port != 80))
    {
      printf("Error: WiFi port has to be greater than 0, but not 80.\n");
      return -1;
    }
  }

  uint16_t wifi_power;
  if (opt_wifi_power)
  {
    wifi_power = (uint8_t)std::stoi(std::string(opt_wifi_power));
    if (wifi_power != 84 && wifi_power != 82 && wifi_power != 80 && wifi_power != 78 && wifi_power != 76 &&
        wifi_power != 74 && wifi_power != 68 && wifi_power != 60 && wifi_power != 52 && wifi_power != 44 &&
        wifi_power != 34 && wifi_power != 28 && wifi_power != 20 && wifi_power != 8)
    {
      {
        printf("Error: WiFi power has to be one of 84, 82, 80, 78, 76, 74, 68, 60, 52, 44, 34, 28, 20, 8.\n");
        return -1;
      }
    }
  }

  uint8_t y_offset;
  if (opt_y_offset)
  {
    y_offset = (uint8_t)std::stoi(std::string(opt_y_offset));
    if (!(y_offset >= 0 && y_offset <= 32))
    {
      printf("Error: y-offset has to be betwen 0 and 32.\n");
      return -1;
    }
  }

  if (opt_port && opt_ip_address)
  {
    printf("Error: provide port or ip-address to connect, but not both..\n");
    return -1;
  }

  ZeDMD* pZeDMD = new ZeDMD();

  if (opt_version)
  {
    printf("zedmd-client version:  %s\n", pZeDMD->GetVersion());
    printf("libzedmd version:      %s\n", pZeDMD->GetVersion());
    printf("libserialport version: %s\n", sp_get_package_version_string());
    delete pZeDMD;
    pZeDMD = nullptr;
    return -1;
  }

  if (opt_verbose)
  {
    pZeDMD->EnableVerbose();
    pZeDMD->SetLogCallback(LogCallback, nullptr);
  }

  if (opt_ip_address)
  {
    if (!pZeDMD->OpenWiFi(opt_ip_address))
    {
      printf("Unable to open connection to ZeDMD via WiFi.\n");
      delete pZeDMD;
      pZeDMD = nullptr;
      return -1;
    }
    wifi = true;
  }
  else if (opt_port)
  {
    pZeDMD->SetDevice(opt_port);

    if (!pZeDMD->Open())
    {
      printf("Unable to open connection to ZeDMD via USB.\n");
      delete pZeDMD;
      pZeDMD = nullptr;
      return -1;
    }
  }
  else
  {
    if (!pZeDMD->Open())
    {
      if (!pZeDMD->OpenDefaultWiFi())
      {
        printf("Unable to open connection to ZeDMD via USB or WiFi.\n");
        delete pZeDMD;
        pZeDMD = nullptr;
        return -1;
      }
      wifi = true;
    }
  }

  // Let the run thread start.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  if (opt_info)
  {
    printf("\nZeDMD Info\n");
    printf("=============================================================\n");
    printf("ID:                         %s\n", pZeDMD->GetIdString());
    printf("firmware version:           %s\n", pZeDMD->GetFirmwareVersion());
    printf("CPU:                        %s\n", pZeDMD->IsS3() ? "ESP32 S3" : "ESP32");
    printf("libzedmd version:           %s\n", pZeDMD->GetVersion());
    printf("transport:                  %d (%s)\n", pZeDMD->GetTransport(),
           pZeDMD->GetTransport() == 0 ? "USB" : (pZeDMD->GetTransport() == 1 ? "UDP" : "TCP"));
    if (pZeDMD->GetTransport() == 0)
    {
      printf("device:                     %s\n", pZeDMD->GetDevice());
    }
    else
    {
      printf("IP address:                 %s\n", pZeDMD->GetIp());
    }
    printf("USB package size:           %d\n", pZeDMD->GetUsbPackageSize());
    if (pZeDMD->GetTransport() == 0)
    {
      printf("WiFi SSID:                  could only be retrieved via WiFi\n");
      printf("WiFi port:                  could only be retrieved via WiFi\n");
      printf("WiFi power:                 could only be retrieved via WiFi\n");
    }
    else
    {
      printf("WiFi SSID:                  %s\n", pZeDMD->GetWiFiSSID());

      printf("WiFi port:                  %d\n", pZeDMD->GetWiFiPort());
      printf("WiFi power:                 %d\n", pZeDMD->GetWiFiPower());
    }
    printf("WiFi UDP delay:             %d\n", pZeDMD->GetUdpDelay());
    printf("panel width:                %d\n", pZeDMD->GetWidth());
    printf("panel height:               %d\n", pZeDMD->GetHeight());
    printf("panel RGB order:            %d\n", pZeDMD->GetRGBOrder());
    printf("panel brightness:           %d\n", pZeDMD->GetBrightness());
    printf("panel clock phase:          %d\n", pZeDMD->GetPanelClockPhase());
    printf("panel i2s speed:            %d\n", pZeDMD->GetPanelI2sSpeed());
    printf("panel latch blanking:       %d\n", pZeDMD->GetPanelLatchBlanking());
    printf("panel minimal refresh rate: %d\n", pZeDMD->GetPanelMinRefreshRate());
    printf("panel driver:               %d\n", pZeDMD->GetPanelDriver());
    printf("Y-offset:                   %d\n\n", pZeDMD->GetYOffset());
  }

  bool save = false;
  if (opt_debug)  // debug should be first to debug commands below
  {
    if (1 == debug)
      pZeDMD->EnableDebug();
    else
      pZeDMD->DisableDebug();
    save = true;
  }
  if (opt_brightness)
  {
    pZeDMD->SetBrightness(brightness);
    save = true;
  }
  if (opt_panel_clkphase)
  {
    pZeDMD->SetPanelClockPhase(panel_clkphase);
    save = true;
  }
  if (opt_panel_driver)
  {
    pZeDMD->SetPanelDriver(panel_driver);
    save = true;
  }
  if (opt_panel_i2sspeed)
  {
    pZeDMD->SetPanelI2sSpeed(panel_i2sspeed);
    save = true;
  }
  if (opt_panel_latch_blanking)
  {
    pZeDMD->SetPanelLatchBlanking(panel_latch_blanking);
    save = true;
  }
  if (opt_panel_min_refresh_rate)
  {
    pZeDMD->SetPanelMinRefreshRate(panel_min_refresh_rate);
    save = true;
  }
  if (opt_rgb_order)
  {
    pZeDMD->SetRGBOrder(rgb_order);
    save = true;
  }
  if (opt_transport)
  {
    pZeDMD->SetTransport(transport);
    save = true;
  }
  if (opt_udp_delay)
  {
    pZeDMD->SetUdpDelay(udp_delay);
    save = true;
  }
  if (opt_usb_package_size)
  {
    pZeDMD->SetUsbPackageSize(usb_package_size);
    save = true;
  }
  if (opt_wifi_ssid)
  {
    pZeDMD->SetWiFiSSID(opt_wifi_ssid);
    save = true;
  }
  if (opt_wifi_password)
  {
    pZeDMD->SetWiFiPassword(opt_wifi_password);
    save = true;
  }
  if (opt_wifi_port)
  {
    pZeDMD->SetWiFiPort(wifi_port);
    save = true;
  }
  if (opt_wifi_power)
  {
    pZeDMD->SetWiFiPower(wifi_power);
    save = true;
  }
  if (opt_y_offset)
  {
    pZeDMD->SetBrightness(brightness);
    save = true;
  }

  if (save)
  {
    pZeDMD->SaveSettings();
    if (wifi)
    {
      pZeDMD->Reset();
    }
    else
    {
      pZeDMD->Close();
    }
  }
  else if (opt_led_test)
  {
    pZeDMD->LedTest();
  }
  else
  {
    pZeDMD->Close();
  }

  delete pZeDMD;
  pZeDMD = nullptr;
  return 0;
}
