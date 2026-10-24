#include "ReduceMotion.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace rtxx::ui {

bool reduceMotion() {
#if defined(__APPLE__)
    // Misma clave que lee NSWorkspace.accessibilityDisplayShouldReduceMotion.
    static const bool value = [] {
        Boolean valid = false;
        const bool on = CFPreferencesGetAppBooleanValue(
            CFSTR("reduceMotion"), CFSTR("com.apple.universalaccess"), &valid);
        return valid && on;
    }();
    return value;
#else
    return false;
#endif
}

}  // namespace rtxx::ui
