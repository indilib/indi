#pragma once

#include <vector>
#include <string>

namespace INDI
{

// Allocate a new uuid for this blob content
std::string allocateBlobUid(int fd);

// Release the blob for which uid has not been attached
void releaseBlobUids(const std::vector<std::string> & uids);

// Attach the given blob buffer and release it's uid
void * attachBlobByUid(const std::string & uid, size_t size);

}