#include <stdarg.h>
#include <stdlib.h>
#include <chrono>
#include <thread>

#include "ZeDMD.h"

void ZEDMDCALLBACK LogCallback(const char* format, va_list args,
                               const void* pUserData) {
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);

  printf("%s\n", buffer);
}

uint8_t* CreateImage(int depth) {
  uint8_t* pImage = (uint8_t*)malloc(128 * 32);
  int pos = 0;
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      pImage[pos++] = x % ((depth == 2) ? 4 : 16);
    }
  }
  return pImage;
}

uint8_t* CreateImageRGB24() {
  uint8_t* pImage = (uint8_t*)malloc(128 * 32 * 3);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 128; ++x) {
      int index = (y * 128 + x) * 3;
      pImage[index++] = (uint8_t)(255 * x / 128);
      pImage[index++] = (uint8_t)(255 * y / 32);
      pImage[index] = (uint8_t)(128);
    }
  }
  return pImage;
}

int main(int argc, const char* argv[]) {
  ZeDMD* pZeDMD = new ZeDMD();
  pZeDMD->SetLogCallback(LogCallback, nullptr);

  if (pZeDMD->Open(128, 32)) {
    //pZeDMD->EnableDebug();

    uint8_t* pImage2 = CreateImage(2);
    uint8_t* pImage4 = CreateImage(4);
    uint8_t* pImage24 = CreateImageRGB24();

    for (int i = 0; i < 3; i++) {
      printf("Render loop: %d\n", i);

      printf("Grey2\n");
      pZeDMD->SetDefaultPalette(2);
      pZeDMD->RenderGray2(pImage2);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("Grey4\n");
      pZeDMD->SetDefaultPalette(4);
      pZeDMD->RenderGray4(pImage4);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("RGB24 Streaming\n");
      pZeDMD->RenderRgb24(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      printf("RGB24 Streaming\n");
      pZeDMD->RenderRgb24(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("RGB24 Streaming\n");
      pZeDMD->RenderRgb24(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();
    }

    pZeDMD->DisableRGB24Streaming();

    for (int i = 0; i < 3; i++) {
      printf("Render loop: %d\n", i);

      printf("Grey2\n");
      pZeDMD->SetDefaultPalette(2);
      pZeDMD->RenderGray2(pImage2);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("Grey4\n");
      pZeDMD->SetDefaultPalette(4);
      pZeDMD->RenderGray4(pImage4);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("RGB24\n");
      pZeDMD->RenderRgb24(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("RGB24 Streaming\n");
      pZeDMD->RenderRgb24(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

    }

    pZeDMD->EnforceStreaming();

    for (int i = 0; i < 3; i++) {
      printf("Streaming render loop: %d\n", i);

      printf("Grey2\n");
      pZeDMD->SetDefaultPalette(2);
      pZeDMD->RenderGray2(pImage2);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("Grey4\n");
      pZeDMD->SetDefaultPalette(4);
      pZeDMD->RenderGray4(pImage4);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("RGB24\n");
      pZeDMD->RenderRgb24(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();

      printf("RGB24 as RGB565\n");
      pZeDMD->RenderRgb24EncodedAs565(pImage24);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      pZeDMD->ClearScreen();
    }

    free(pImage2);
    free(pImage4);
    free(pImage24);

    pZeDMD->LedTest();

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    pZeDMD->Close();
  }
}
