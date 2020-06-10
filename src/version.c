#include "version.h"

#define __BDATE__      "2020-06-09 14:55:37"
#define __BVERSION__   "1.0.004"

const char * getVersion()
{
    return __BVERSION__;
}

const char * getBuildDate()
{
    return __BDATE__;
}
