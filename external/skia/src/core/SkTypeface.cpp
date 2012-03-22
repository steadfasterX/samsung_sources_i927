#include "SkTypeface.h"
#include "SkFontHost.h"

#ifdef MONOTYPE_FLIPFONT
#include <utils/Log.h>	// debug output
extern uint32_t oldSansBoldUID, newSansBoldUID;
extern uint32_t oldSansUID, newSansUID;
extern SkMutex ffMutex;

extern bool gFontHasJustBeenFlipped;
#endif // MONOTYPE_FLIPFONT

uint32_t SkTypeface::UniqueID(const SkTypeface* face) {
#ifdef MONOTYPE_FLIPFONT

    SkAutoMutexAcquire    ac(ffMutex);
    ac.release();           // release the mutex now

    if (face) {
        uint32_t retVal;
        retVal = face->uniqueID();
        if(retVal == oldSansBoldUID) {
             //__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "UniqueID(), changing %d to %d (bold)", retVal,  newSansBoldUID);

             retVal = newSansBoldUID;
             }
        if(retVal == oldSansUID) {
             //__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "UniqueID(), changing %d to %d (sans)", retVal,  newSansUID);

             retVal = newSansUID;
             }
        return retVal;
    }
#else
    if (face) {
        return face->uniqueID();
    }
#endif

    // We cache the default fontID, assuming it will not change during a boot
    // The initial value of 0 is fine, since a typeface's uniqueID should not
    // be zero.
    static uint32_t gDefaultFontID;
    
#ifdef MONOTYPE_FLIPFONT
    if (0 == gDefaultFontID || gFontHasJustBeenFlipped) {
        gFontHasJustBeenFlipped = false;
#else
    if (0 == gDefaultFontID) {
#endif
        
        SkTypeface* defaultFace = SkFontHost::CreateTypeface(NULL, NULL,
                                                    SkTypeface::kNormal);
        SkASSERT(defaultFace);
        gDefaultFontID = defaultFace->uniqueID();
        defaultFace->unref();
    }
    return gDefaultFontID;
}

bool SkTypeface::Equal(const SkTypeface* facea, const SkTypeface* faceb) {
    return SkTypeface::UniqueID(facea) == SkTypeface::UniqueID(faceb);
}

///////////////////////////////////////////////////////////////////////////////

SkTypeface* SkTypeface::CreateFromName(const char name[], Style style) {
    return SkFontHost::CreateTypeface(NULL, name, style);
}

SkTypeface* SkTypeface::CreateFromTypeface(const SkTypeface* family, Style s) {
    return SkFontHost::CreateTypeface(family, NULL, s);
}

SkTypeface* SkTypeface::CreateFromStream(SkStream* stream) {
    return SkFontHost::CreateTypefaceFromStream(stream);
}

//SAMSUNG CHANGE >>
/** Returns true if the given stream is a valid typeface stream
*/
bool SkTypeface::isValidStream(SkStream* stream) {
    return SkFontHost::isValidStream(stream);
}
//SAMSUNG CHANGE <<


SkTypeface* SkTypeface::CreateFromFile(const char path[]) {
    return SkFontHost::CreateTypefaceFromFile(path);
}

///////////////////////////////////////////////////////////////////////////////

void SkTypeface::serialize(SkWStream* stream) const {
    SkFontHost::Serialize(this, stream);
}

SkTypeface* SkTypeface::Deserialize(SkStream* stream) {
    return SkFontHost::Deserialize(stream);
}


