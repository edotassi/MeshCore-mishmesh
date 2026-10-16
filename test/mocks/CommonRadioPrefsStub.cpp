// Host stand-in for src/helpers/CommonRadioPrefs.cpp, which pulls in the board's
// target.h (radio_driver) and so cannot build natively. Only the out-of-line
// virtuals are needed to emit the vtable for tests that instantiate NodePrefs.
#include <helpers/CommonRadioPrefs.h>

bool CommonRadioPrefs::getByKey(const char*, char*, size_t) { return false; }
bool CommonRadioPrefs::setByKey(const char*, const char*) { return false; }
