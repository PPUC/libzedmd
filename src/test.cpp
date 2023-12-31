#include "ZeDMD.h"

#include <chrono>
#include <thread>

int main(int argc, const char *argv[])
{
   ZeDMD *pZEDMD = new ZeDMD();

   if (pZEDMD->Open())
   {
      pZEDMD->LedTest();

      std::this_thread::sleep_for(std::chrono::milliseconds(5000));

      pZEDMD->Close();
   }
}