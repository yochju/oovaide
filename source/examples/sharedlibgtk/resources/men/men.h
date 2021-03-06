// men.h

#include <gmodule.h>
#if defined(SHARED_LIBRARY)
#  define SHAREDSHARED_EXPORT G_MODULE_EXPORT
#else
#  define SHAREDSHARED_EXPORT G_MODULE_IMPORT
#endif


extern "C"
    {
    SHAREDSHARED_EXPORT void getResourceName(char *buf, int maxBytes);
    SHAREDSHARED_EXPORT bool putTogether();
    };

/// This defines the Men resource
class Men
    {
    public:
        /// The Men resource has its own name.
        char const * const getName() const;
        /// Returns false, since there is no way to put men together.
        bool putTogether();
    };
