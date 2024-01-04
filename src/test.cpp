#include "ZeDMD.h"

#include <stdarg.h>
#include <chrono>
#include <thread>

void CALLBACK OnLogMessage(const char* format, va_list args, const void* pUserData)
{
   char buffer[1024];
   vsnprintf(buffer, sizeof(buffer), format, args);

   printf("%s\n", buffer);
}

int main(int argc, const char *argv[])
{
   ZeDMD* pZEDMD = new ZeDMD();
   pZEDMD->SetLogMessageCallback(OnLogMessage, NULL);

   if (pZEDMD->Open())
   {
      pZEDMD->LedTest();

      std::this_thread::sleep_for(std::chrono::milliseconds(5000));

      pZEDMD->Close();
   }
}