#ifndef INDIBASE_H
#define INDIBASE_H

#include <vector>
#include <map>
#include <string>

#include "indiapi.h"
#include "indidevapi.h"

class INDIBaseDevice;

#define MAXRBUF 2048

using namespace std;

namespace INDI
{
    class BaseDevice;
    class BaseClient;
    class DefaultDevice;
}

#endif // INDIBASE_H
