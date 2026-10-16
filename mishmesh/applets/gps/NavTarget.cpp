#include <mishmesh/applets/gps/NavTarget.h>
#include <mishmesh/core/StrUtil.h>

namespace mishmesh {

void NavTarget::set(const char* name, int32_t lat1e6, int32_t lon1e6) {
  copyStr(_name, sizeof(_name), name ? name : "");
  _lat = lat1e6; _lon = lon1e6;
  _active = true; _armed = false;
}

NavTarget& navTarget() {
  static NavTarget t;
  return t;
}

}  // namespace mishmesh
