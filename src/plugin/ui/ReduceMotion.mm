#include "ReduceMotion.h"

#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#endif

namespace rtxx::ui {

bool reduceMotion() {
#if defined(__APPLE__)
    // API documentada; leer com.apple.universalaccess con CFPreferences falla en hosts con sandbox.
    static const bool value = [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
    return value;
#else
    return false;
#endif
}

}  // namespace rtxx::ui
