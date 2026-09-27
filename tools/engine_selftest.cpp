// The engine self check where the console (WinMM) does not build: what fsvr_console -selftest runs.
#include "fs1r.h"

int main() { return fs1r::Device::selfTest() == 0 ? 0 : 1; }
