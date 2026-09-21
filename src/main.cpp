#include <pspkernel.h>
#include "app.hpp"

PSP_MODULE_INFO("ADShare", 0, 2, 4);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

int main()
{
    adshare::Application app;
    return app.run();
}
