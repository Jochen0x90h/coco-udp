#pragma once

#ifdef _WIN32
#include "UdpSocket_Win32.hpp"
namespace coco {
using UdpSocket_native = UdpSocket_Win32;
}
#endif
