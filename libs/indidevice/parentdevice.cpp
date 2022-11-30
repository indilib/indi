#include "parentdevice.h"
#include "parentdevice_p.h"

#include "basedevice.h"
#include "basedevice_p.h"

#include <atomic>

namespace INDI
{

static std::shared_ptr<ParentDevicePrivate> create(bool valid)
{
    class InvalidParentDevicePrivate: public ParentDevicePrivate
    {
        public:
            InvalidParentDevicePrivate()
            {
                this->valid = false;
            }
    };

    if (valid == false)
    {
        static InvalidParentDevicePrivate invalidDevice;
        return make_shared_weak(&invalidDevice);
    }
    else
    {
        std::shared_ptr<ParentDevicePrivate> validDevice(new ParentDevicePrivate);
        return validDevice;
    }
}


ParentDevice::ParentDevice(ParentDevice::Type type)
    : BaseDevice(std::shared_ptr<BaseDevicePrivate>(create(type == Valid)))
{
    D_PTR(ParentDevice);
    ++d->ref;
}

ParentDevice::ParentDevice(const std::shared_ptr<ParentDevicePrivate> &dd)
    : BaseDevice(std::shared_ptr<BaseDevicePrivate>(dd))
{
    D_PTR(ParentDevice);
    ++d->ref;
}

ParentDevice::~ParentDevice()
{
    D_PTR(ParentDevice);
    if (--d->ref == 0)
    {
        // prevent circular reference
        d->pAll.clear();
    }
}

}
