#include <stdarg.h>
#include <stdlib.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "ZeDMD.h"

void ZEDMDCALLBACK LogCallback(const char* format, va_list args, const void* pUserData)
{
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);

  printf("%s\n", buffer);
}

uint8_t* CreateImageRGB24()
{
  uint8_t* pImage = (uint8_t*)malloc(128 * 32 * 3);
  for (int y = 0; y < 32; ++y)
  {
    for (int x = 0; x < 128; ++x)
    {
      int index = (y * 128 + x) * 3;
      pImage[index++] = (uint8_t)(255 * x / 128);
      pImage[index++] = (uint8_t)(255 * y / 32);
      pImage[index] = (uint8_t)(128);
    }
  }
  return pImage;
}

int main(int argc, const char* argv[])
{
  ZeDMD* pZeDMD = new ZeDMD();
  pZeDMD->SetLogCallback(LogCallback, nullptr);

  if (pZeDMD->Open() || pZeDMD->OpenDefaultWiFi())
  {
    pZeDMD->EnableDebug();
    uint16_t width = pZeDMD->GetWidth();
    uint16_t height = pZeDMD->GetHeight();

    pZeDMD->SetFrameSize(128, 32);

    uint8_t* pImage24 = CreateImageRGB24();
    uint16_t sleep = 16;

    pZeDMD->EnableUpscaling();

    for (int i = 0; i < 3; i++)
    {
      printf("Render loop: %d\n", i);
      printf("RGB24 Streaming\n");
      pZeDMD->RenderRgb888(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
      pZeDMD->ClearScreen();
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }

    pZeDMD->DisableUpscaling();

    for (int i = 0; i < 10; i++)
    {
      printf("Render loop: %d\n", i);
      printf("RGB24 Streaming\n");
      pZeDMD->RenderRgb888(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
      pZeDMD->ClearScreen();
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }

    free(pImage24);

    pZeDMD->SetFrameSize(width, height);
    FILE* fileptr;
    uint16_t size = width * height * 2;
    uint8_t* buffer = (uint8_t*)malloc(size * sizeof(uint8_t));
    uint16_t* rgb565 = (uint16_t*)malloc(size / 2 * sizeof(uint16_t));
    char filename[34];

    for (int i = 1; i <= 100; i++)
    {
      snprintf(filename, 33, "test/rgb565_%dx%d/%04d.raw", width, height, i);
      printf("Render raw: %s\n", filename);
      fileptr = fopen(filename, "rb");

      if (fileptr == NULL)
      {
        printf("Failed to open file, make sure to copy the test folder with the RAW files!");
        break;
      }

      fread(buffer, size, 1, fileptr);
      fclose(fileptr);

      memcpy(rgb565, buffer, size);
      pZeDMD->RenderRgb565(rgb565);
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }

    if (width == 256)
    {
      // test RGB565 upscaling
      size = 128 * 32 * 2;
      pZeDMD->SetFrameSize(128, 32);

      for (int i = 1; i <= 20; i++)
      {
        snprintf(filename, 33, "test/rgb565_128x32/%04d.raw", i);
        printf("Render raw: %s\n", filename);
        fileptr = fopen(filename, "rb");
        fread(buffer, size, 1, fileptr);
        fclose(fileptr);

        memcpy(rgb565, buffer, size);
        pZeDMD->RenderRgb565(rgb565);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
      }

      // test RGB565 centering
      pZeDMD->DisableUpscaling();

      for (int i = 1; i <= 100; i++)
      {
        snprintf(filename, 33, "test/rgb565_128x32/%04d.raw", i);
        printf("Render raw: %s\n", filename);
        fileptr = fopen(filename, "rb");
        fread(buffer, size, 1, fileptr);
        fclose(fileptr);

        memcpy(rgb565, buffer, size);
        pZeDMD->RenderRgb565(rgb565);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
      }

      pZeDMD->SetFrameSize(width, height);
    }

    free(buffer);
    free(rgb565);

    size = width * height * 3;
    uint8_t* rgb888 = (uint8_t*)malloc(size * sizeof(uint8_t));

    for (int i = 1; i <= 100; i++)
    {
      snprintf(filename, 33, "test/rgb888_%dx%d/%04d.raw", width, height, i);
      printf("Render raw: %s\n", filename);
      fileptr = fopen(filename, "rb");
      fread(rgb888, size, 1, fileptr);
      fclose(fileptr);

      pZeDMD->RenderRgb888(rgb888);
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }

    free(rgb888);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    pZeDMD->LedTest();

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    pZeDMD->Close();
  }
}
