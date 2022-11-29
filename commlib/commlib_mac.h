#pragma once

#include <string>

namespace commlib
{

/** returns the path to the lock file; creates a directory for it if necessary;
 throws if directory creation failed
*/
std::string lockFileName();

std::string socketFileName(bool aDeleteSocket);

}
