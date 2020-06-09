#include "version.h"

#define __BDATE__      "2020-06-07 12:21:38"
#define __BVERSION__   "1.0.003"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
