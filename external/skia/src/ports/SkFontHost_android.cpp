/* libs/graphics/ports/SkFontHost_android.cpp
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include "SkFontHost.h"
#include "SkDescriptor.h"
#include "SkMMapStream.h"
#include "SkPaint.h"
#include "SkString.h"
#include "SkStream.h"
#include "SkThread.h"
#include "SkTSearch.h"
#include <stdio.h>

#ifdef MONOTYPE_FLIPFONT
#include "SkGlyphCache.h"
#include <utils/Log.h>	// debug output
#endif

#define FONT_CACHE_MEMORY_BUDGET    (768 * 1024)

#ifndef SK_FONT_FILE_PREFIX
    #define SK_FONT_FILE_PREFIX          "/fonts/"
#endif

#ifdef MONOTYPE_FLIPFONT

#ifndef SK_FONT_MONOTYPE_LOCATION
    #define SK_FONT_MONOTYPE_LOCATION "/data/data/com.android.settings/app_fonts/"
    #define SK_FONT_MONOTYPE_LOCATION2 "/system/fonts/"
#endif
#define SK_FONT_MONOTYPE_SANS  "sans.loc"
#define SK_FONT_MONOTYPE_MONO  "monospace.loc"
#define SK_FONT_MONOTYPE_SERIF "serif.loc"

#define MONOTYPE_VERSION       "Monotype Imaging Inc. Version 1.3"

#define YOON_COPYRIGHT  "Yoon Design Inc. All Rights Reserved."
#define MONOTYPE_COPYRIGHT  "Monotype Imaging Inc."
#define YOON_FONTNAME_1  "Choco cooky"
#define YOON_FONTNAME_2  "Rosemary"
#define YOON_FONTNAME_3  "Cool jazz"
#define YOON_FONTNAME_4  "Applemint"
#define YOON_FONTNAME_5  "Tinkerbell"
#define YOON_FONTNAME_6  "Armani"
#define YOON_FONTNAME_7  "Armani-Bold"


char FlipFontPath[256]; 
uint32_t oldSansUID, oldSansBoldUID;
uint32_t newSansUID, newSansBoldUID;
SkMutex ffMutex, lsfMutex;

#if 0
SkTypeface *oldSansBoldTypeface = NULL;
SkTypeface *olderSansBoldTypeface = NULL;
#endif
#define NUM_OLD_BOLD_FACES 64
SkTypeface *oldSansBoldTypefaces[NUM_OLD_BOLD_FACES]; // we'll remember many old Typefaces to do proper pseudo-bolding

#define SWAPWORD(p) (((p & 0xff00)>>8) + ((p & 0xff)<<8))
#define SWAPLONG(p) (((p & 0xff000000)>>24) + ((p & 0xff0000)>>8) + ((p & 0xff00)<<8) + ((p & 0xff)<<24))

typedef struct {
  short platform;
  short encoding;
  short language;
  short nameid;
  short strlen;
  short stroffset;
} namerecord_type;

typedef struct {
  short format;
  short numrecords;
  short stroffset;
  namerecord_type record[1];
} nametable_type;

typedef struct {
  unsigned long tag;
  unsigned long checksum;
  unsigned long offset;
  unsigned long size;
} table_type;

typedef struct {
  long version;
  short numtables;
  short searchRange;
  short entrySelector;
  short rangeShift;
  table_type tables[1];
} truetype_type;

int getCopyright(char *buffer, char **copyright);
bool CopyrightAndNameAreValid(FILE *fp);

// Possible font requests, to matched to above .loc files
char* fonts[] = {
		"DroidSans.ttf", "DroidSans-Bold.ttf", "DroidSansMono.ttf",
		"DroidSerif-Regular.ttf", "DroidSerif-Bold.ttf",
		"DroidSerif-BoldItalic.ttf", "DroidSerif-Italic.ttf", NULL };

// Mapping to .loc files according to this enum
enum FontTypes {
	EFontType_Sans = 0,
	EFontType_SansBold,
	EFontType_Mono,
	EFontType_Serif,
	EFontType_SerifBold,
	EFontType_SerifBoldItalic,
	EFontType_SerifItalic,
	EFontType_Undefined,
};

// We need the slash in the directory
#define SK_FONT_MONOTYPE_SLASH "/"

uint32_t gFlipFontID = NULL;
bool gFontHasJustBeenFlipped = false;

#endif // MONOTYPE_FLIPFONT

bool find_name_and_style(SkStream* stream, SkString* name, SkTypeface::Style* style);
bool is_valid_stream(SkStream* stream); //SAMSUNG CHANGE

static void GetFullPathForSysFonts(SkString* full, const char name[]) {

#ifdef MONOTYPE_FLIPFONT
	FILE *fp;


	// Check which font we want
	int fonttype = EFontType_Undefined;
	char fontdir[128];
	memset(fontdir, 0, 128);
	// Determine the font type > .loc to use
	for(int font = 0; font < EFontType_Undefined; font ++) {
		if (strncmp(name, fonts[font], strlen(fonts[font])) == 0) {
			fonttype = font;
		}
	}
	//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "SKIA_FONT request font: %s, %d", name, fonttype);
	if (fonttype < EFontType_Undefined ) {
		char *fileline;
		strcpy(fontdir, SK_FONT_MONOTYPE_LOCATION);
		if( fonttype <= EFontType_SansBold ) {
			strcat(fontdir, SK_FONT_MONOTYPE_SANS);
		} else if (fonttype <= EFontType_Mono ) {
			strcat(fontdir, SK_FONT_MONOTYPE_MONO);
		} else if (fonttype <= EFontType_SerifItalic ) {
			strcat(fontdir, SK_FONT_MONOTYPE_SERIF);
		}
		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "SKIA_FONT checking file: %s", fontdir);
		// Retrieve the font directory
		fp = fopen(fontdir, "r");
		if (fp == NULL) {
			strcpy(fontdir, SK_FONT_MONOTYPE_LOCATION2);
		    if( fonttype <= EFontType_SansBold ) {
			    strcat(fontdir, SK_FONT_MONOTYPE_SANS);
		    } else if (fonttype <= EFontType_Mono ) {
			    strcat(fontdir, SK_FONT_MONOTYPE_MONO);
		    } else if (fonttype <= EFontType_SerifItalic ) {
			    strcat(fontdir, SK_FONT_MONOTYPE_SERIF);
		    }
		    fp = fopen(fontdir, "r");
		}
		
		if (fp != NULL) {
			memset(fontdir, 0 , 128);
			fileline = fgets(fontdir, 128, fp);
			//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "SKIA_FONT read: %s", fontdir);
			// Close the file
			fclose(fp);
			if (fileline != NULL) {
				int lastchar = strlen(fileline);
				if (lastchar > 0) {
					if (fileline[lastchar-1] == '\n' ||
							fileline[lastchar-1] == '\r' ) {
						// Remove the newline
						lastchar--;
						fileline[lastchar] = '\0';
					}
					if (fileline[lastchar-1] != '\/') {
						strcat(fontdir, SK_FONT_MONOTYPE_SLASH);
					}
					// Check if the file in the directory exists
					strcat(fontdir, name);
					fp = fopen(fontdir, "r");
					__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "SKIA_FONT fontfile: %s", fontdir);
					if (fp != NULL)
					{
						int filelen;
						fseek(fp,0,SEEK_END);
						filelen = ftell(fp);
						if(filelen) {
						if(CopyrightAndNameAreValid(fp)){	
						    fclose(fp);
								//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Copyright and name valid");	
						    // Yes we have a valid file
						    full->set(fontdir);				
						    // Okay we found our font file is set
						    return;
						}	
							else {
								/* bad copyright. delete it */
						fclose(fp);					    						
								remove(fontdir);
								__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Copyright and name NOT valid");	
					}
						} else {
							/* empty file. delete it */
							fclose(fp);	
							remove(fontdir);
				}
					} // fp != NULL
				} //lastchar > 0
			} // fileline != NULL
		} // fp != NULL
	} // fonttype < EFontType_Undefined
	// If this didn't work out, use the default system fonts
#endif // MONOTYPE_FLIPFONT

    full->set(getenv("ANDROID_ROOT"));
    full->append(SK_FONT_FILE_PREFIX);
    full->append(name);
}

///////////////////////////////////////////////////////////////////////////////

struct FamilyRec;

/*  This guy holds a mapping of a name -> family, used for looking up fonts.
    Since it is stored in a stretchy array that doesn't preserve object
    semantics, we don't use constructor/destructors, but just have explicit
    helpers to manage our internal bookkeeping.
*/
struct NameFamilyPair {
    const char* fName;      // we own this
    FamilyRec*  fFamily;    // we don't own this, we just reference it
    
    void construct(const char name[], FamilyRec* family) {
        fName = strdup(name);
        fFamily = family;   // we don't own this, so just record the referene
    }

    void destruct() {
        free((char*)fName);
        // we don't own family, so just ignore our reference
    }
};

// we use atomic_inc to grow this for each typeface we create
static int32_t gUniqueFontID;

// this is the mutex that protects these globals
static SkMutex gFamilyMutex;
static FamilyRec* gFamilyHead;
static SkTDArray<NameFamilyPair> gNameList;

struct FamilyRec {
    FamilyRec*  fNext;
    SkTypeface* fFaces[4];
    
    FamilyRec()
    {
        fNext = gFamilyHead;
        memset(fFaces, 0, sizeof(fFaces));
        gFamilyHead = this;
    }
};

static SkTypeface* find_best_face(const FamilyRec* family,
                                  SkTypeface::Style style) {
    SkTypeface* const* faces = family->fFaces;

    if (faces[style] != NULL) { // exact match
        return faces[style];
    }
    // look for a matching bold
    style = (SkTypeface::Style)(style ^ SkTypeface::kItalic);
    if (faces[style] != NULL) {
        return faces[style];
    }
    // look for the plain
    if (faces[SkTypeface::kNormal] != NULL) {
        return faces[SkTypeface::kNormal];
    }
    // look for anything
    for (int i = 0; i < 4; i++) {
        if (faces[i] != NULL) {
            return faces[i];
        }
    }
    // should never get here, since the faces list should not be empty
    SkASSERT(!"faces list is empty");
    return NULL;
}

static FamilyRec* find_family(const SkTypeface* member) {
    FamilyRec* curr = gFamilyHead;
    while (curr != NULL) {
        for (int i = 0; i < 4; i++) {
            if (curr->fFaces[i] == member) {
                return curr;
            }
        }
        curr = curr->fNext;
    }
    return NULL;
}

/*  Returns the matching typeface, or NULL. If a typeface is found, its refcnt
    is not modified.
 */
static SkTypeface* find_from_uniqueID(uint32_t uniqueID) {
    FamilyRec* curr = gFamilyHead;
    while (curr != NULL) {
        for (int i = 0; i < 4; i++) {
            SkTypeface* face = curr->fFaces[i];
            if (face != NULL && face->uniqueID() == uniqueID) {
                return face;
            }
        }
        curr = curr->fNext;
    }
    return NULL;
}

/*  Remove reference to this face from its family. If the resulting family
    is empty (has no faces), return that family, otherwise return NULL
*/
static FamilyRec* remove_from_family(const SkTypeface* face) {
    FamilyRec* family = find_family(face);
    SkASSERT(family->fFaces[face->style()] == face);
    family->fFaces[face->style()] = NULL;
    
    for (int i = 0; i < 4; i++) {
        if (family->fFaces[i] != NULL) {    // family is non-empty
            return NULL;
        }
    }
    return family;  // return the empty family
}

// maybe we should make FamilyRec be doubly-linked
static void detach_and_delete_family(FamilyRec* family) {
    FamilyRec* curr = gFamilyHead;
    FamilyRec* prev = NULL;

    while (curr != NULL) {
        FamilyRec* next = curr->fNext;
        if (curr == family) {
            if (prev == NULL) {
                gFamilyHead = next;
            } else {
                prev->fNext = next;
            }
            SkDELETE(family);
            return;
        }
        prev = curr;
        curr = next;
    }
    SkASSERT(!"Yikes, couldn't find family in our list to remove/delete");
}

static SkTypeface* find_typeface(const char name[], SkTypeface::Style style) {
    NameFamilyPair* list = gNameList.begin();
    int             count = gNameList.count();
    
    int index = SkStrLCSearch(&list[0].fName, count, name, sizeof(list[0]));

    if (index >= 0) {
        return find_best_face(list[index].fFamily, style);
    }
    return NULL;
}

static SkTypeface* find_typeface(const SkTypeface* familyMember,
                                 SkTypeface::Style style) {
    const FamilyRec* family = find_family(familyMember);
    return family ? find_best_face(family, style) : NULL;
}

static void add_name(const char name[], FamilyRec* family) {
    SkAutoAsciiToLC tolc(name);
    name = tolc.lc();

    NameFamilyPair* list = gNameList.begin();
    int             count = gNameList.count();
    
    int index = SkStrLCSearch(&list[0].fName, count, name, sizeof(list[0]));

    if (index < 0) {
        list = gNameList.insert(~index);
        list->construct(name, family);
    }
}

static void remove_from_names(FamilyRec* emptyFamily)
{
#ifdef SK_DEBUG
    for (int i = 0; i < 4; i++) {
        SkASSERT(emptyFamily->fFaces[i] == NULL);
    }
#endif

    SkTDArray<NameFamilyPair>& list = gNameList;
    
    // must go backwards when removing
    for (int i = list.count() - 1; i >= 0; --i) {
        NameFamilyPair* pair = &list[i];
        if (pair->fFamily == emptyFamily) {
            pair->destruct();
            list.remove(i);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

class FamilyTypeface : public SkTypeface {
public:
    FamilyTypeface(Style style, bool sysFont, SkTypeface* familyMember)
    : SkTypeface(style, sk_atomic_inc(&gUniqueFontID) + 1) {
        fIsSysFont = sysFont;
        
        SkAutoMutexAcquire  ac(gFamilyMutex);
        
        FamilyRec* rec = NULL;
        if (familyMember) {
            rec = find_family(familyMember);
            SkASSERT(rec);
        } else {
            rec = SkNEW(FamilyRec);
        }
        rec->fFaces[style] = this;
    }
    
    virtual ~FamilyTypeface() {
        SkAutoMutexAcquire  ac(gFamilyMutex);
        
        // remove us from our family. If the family is now empty, we return
        // that and then remove that family from the name list
        FamilyRec* family = remove_from_family(this);
        if (NULL != family) {
            remove_from_names(family);
            detach_and_delete_family(family);
        }
    }
    
    bool isSysFont() const { return fIsSysFont; }
    
    virtual SkStream* openStream() = 0;
    virtual const char* getUniqueString() const = 0;
    virtual const char* getFilePath() const = 0;
    
private:
    bool    fIsSysFont;
    
    typedef SkTypeface INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class StreamTypeface : public FamilyTypeface {
public:
    StreamTypeface(Style style, bool sysFont, SkTypeface* familyMember,
                   SkStream* stream)
    : INHERITED(style, sysFont, familyMember) {
        SkASSERT(stream);
        stream->ref();
        fStream = stream;
    }
    virtual ~StreamTypeface() {
        fStream->unref();
    }
    
    // overrides
    virtual SkStream* openStream() {
        // we just ref our existing stream, since the caller will call unref()
        // when they are through
        fStream->ref();
        return fStream;
    }
    virtual const char* getUniqueString() const { return NULL; }
    virtual const char* getFilePath() const { return NULL; }

private:
    SkStream* fStream;
    
    typedef FamilyTypeface INHERITED;
};

class FileTypeface : public FamilyTypeface {
public:
    FileTypeface(Style style, bool sysFont, SkTypeface* familyMember,
                 const char path[])
    : INHERITED(style, sysFont, familyMember) {
        SkString fullpath;
        
        if (sysFont) {
            GetFullPathForSysFonts(&fullpath, path);
            path = fullpath.c_str();
        }
        fPath.set(path);
    }
    
    // overrides
    virtual SkStream* openStream() {
        SkStream* stream = SkNEW_ARGS(SkMMAPStream, (fPath.c_str()));

        // check for failure
        if (stream->getLength() <= 0) {
            SkDELETE(stream);
            // maybe MMAP isn't supported. try FILE
            stream = SkNEW_ARGS(SkFILEStream, (fPath.c_str()));
            if (stream->getLength() <= 0) {
                SkDELETE(stream);
                stream = NULL;
            }
        }
        return stream;
    }
    virtual const char* getUniqueString() const {
        const char* str = strrchr(fPath.c_str(), '/');
        if (str) {
            str += 1;   // skip the '/'
        }
        return str;
    }
    virtual const char* getFilePath() const {
        return fPath.c_str();
    }

private:
    SkString fPath;
    
    typedef FamilyTypeface INHERITED;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static bool get_name_and_style(const char path[], SkString* name,
                               SkTypeface::Style* style, bool isExpected) {
    SkString        fullpath;
    GetFullPathForSysFonts(&fullpath, path);

    SkMMAPStream stream(fullpath.c_str());
    if (stream.getLength() > 0) {
        return find_name_and_style(&stream, name, style);
    }
    else {
        SkFILEStream stream(fullpath.c_str());
        if (stream.getLength() > 0) {
            return find_name_and_style(&stream, name, style);
        }
    }

    if (isExpected) {
        SkDebugf("---- failed to open <%s> as a font\n", fullpath.c_str());
    }
    return false;
}

// used to record our notion of the pre-existing fonts
struct FontInitRec {
    const char*         fFileName;
    const char* const*  fNames;     // null-terminated list
};

static const char* gSansNames[] = {
    "sans-serif", "arial", "helvetica", "tahoma", "verdana", NULL
};

static const char* gSerifNames[] = {
    "serif", "times", "times new roman", "palatino", "georgia", "baskerville",
    "goudy", "fantasy", "cursive", "ITC Stone Serif", NULL
};

static const char* gMonoNames[] = {
    "monospace", "courier", "courier new", "monaco", NULL
};

// deliberately empty, but we use the address to identify fallback fonts
static const char* gFBNames[] = { NULL };

/*  Fonts must be grouped by family, with the first font in a family having the
    list of names (even if that list is empty), and the following members having
    null for the list. The names list must be NULL-terminated
*/
static const FontInitRec gSystemFonts[] = {
    { "DroidSans.ttf",              gSansNames  },
    { "DroidSans-Bold.ttf",         NULL        },
    { "DroidSerif-Regular.ttf",     gSerifNames },
    { "DroidSerif-Bold.ttf",        NULL        },
    { "DroidSerif-Italic.ttf",      NULL        },
    { "DroidSerif-BoldItalic.ttf",  NULL        },
    { "DroidSansMono.ttf",          gMonoNames  },
    /*  These are optional, and can be ignored if not found in the file system.
        These are appended to gFallbackFonts[] as they are seen, so we list
        them in the order we want them to be accessed by NextLogicalFont().
     */
    { "DroidSansArabic.ttf",        gFBNames    },
    { "DroidSansHebrew.ttf",        gFBNames    },
    { "DroidSansThai.ttf",          gFBNames    },
    { "MTLmr3m.ttf",                gFBNames    }, // Motoya Japanese Font
    { "MTLc3m.ttf",                 gFBNames    }, // Motoya Japanese Font
    { "DroidSansJapanese.ttf",      gFBNames    },
    { "truetypeEmoji.ttf",      	gFBNames    },
    { "DroidSans_Subset.ttf",       gFBNames    },
    { "DroidSansFallback.ttf",      gFBNames    }
};

#define DEFAULT_NAMES   gSansNames

// these globals are assigned (once) by load_system_fonts()
static FamilyRec* gDefaultFamily;
static SkTypeface* gDefaultNormal;

/*  This is sized conservatively, assuming that it will never be a size issue.
    It will be initialized in load_system_fonts(), and will be filled with the
    fontIDs that can be used for fallback consideration, in sorted order (sorted
    meaning element[0] should be used first, then element[1], etc. When we hit
    a fontID==0 in the array, the list is done, hence our allocation size is
    +1 the total number of possible system fonts. Also see NextLogicalFont().
 */
static uint32_t gFallbackFonts[SK_ARRAY_COUNT(gSystemFonts)+1];

/*  Called once (ensured by the sentinel check at the beginning of our body).
    Initializes all the globals, and register the system fonts.
 */
static void load_system_fonts() {
    // check if we've already be called

    SkAutoMutexAcquire    ac(lsfMutex);
    bool pseudo_bold = false;
    
    if (NULL == gDefaultNormal) {


      
    
    const FontInitRec* rec = gSystemFonts;
    SkTypeface* firstInFamily = NULL;
    int fallbackCount = 0;

    for (size_t i = 0; i < SK_ARRAY_COUNT(gSystemFonts); i++) {
        // if we're the first in a new family, clear firstInFamily
        if (rec[i].fNames != NULL) {
            firstInFamily = NULL;
        }
        
        SkString name;
        SkTypeface::Style style;

        // we expect all the fonts, except the "fallback" fonts
        bool isExpected = (rec[i].fNames != gFBNames);
        if (!get_name_and_style(rec[i].fFileName, &name, &style, isExpected)) {
            continue;
        }
            // OK, we need for there to be a separate Typeface for sans regular and sans bold, 
            //    even when the FlipFont has not corresponding natural bold font. This is because
            //    in PrepareFontFlip() we change the style of all former bold typefaces so that
            //    apps which were renning at the time of the flip do bold correctly.
            if(strcmp(rec[i].fFileName, "DroidSans-Bold.ttf") == 0)
                if(style == SkTypeface::kNormal) {
                    style = SkTypeface::kBold;
                    pseudo_bold = true;
                }
                   



        SkTypeface* tf = SkNEW_ARGS(FileTypeface,
                                    (style,
                                     true,  // system-font (cannot delete)
                                     firstInFamily, // what family to join
                                     rec[i].fFileName) // filename
                                    );

            __android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "load_system_fonts(), name = %s, style = %d, uniqueID = %d" , 

                    name.c_str(), style, tf->uniqueID());

            if(pseudo_bold && strcmp(rec[i].fFileName, "DroidSans-Bold.ttf") == 0)
                tf->setBold(false);



        if (rec[i].fNames != NULL) {
            // see if this is one of our fallback fonts
            if (rec[i].fNames == gFBNames) {
            //    SkDebugf("---- adding %s as fallback[%d] fontID %d\n",
            //             rec[i].fFileName, fallbackCount, tf->uniqueID());
                gFallbackFonts[fallbackCount++] = tf->uniqueID();
            }

            firstInFamily = tf;
            FamilyRec* family = find_family(tf);
            const char* const* names = rec[i].fNames;

            // record the default family if this is it
            if (names == DEFAULT_NAMES) {
                gDefaultFamily = family;
            }
            // add the names to map to this family
            while (*names) {
                add_name(*names, family);
                names += 1;
            }
        }
    }

    // do this after all fonts are loaded. This is our default font, and it
    // acts as a sentinel so we only execute load_system_fonts() once
    gDefaultNormal = find_best_face(gDefaultFamily, SkTypeface::kNormal);
        
        SkFontHost::FetchSansUniqueIDs(&newSansUID, &newSansBoldUID);

        

        __android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "load_system_fonts(), oldSansUID = %d, newSansUID = %d" , 

                    oldSansUID, newSansUID);

        __android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "load_system_fonts(), oldSansBoldUID = %d, newSansBoldUID = %d" , 

                    oldSansBoldUID, newSansBoldUID);



    // now terminate our fallback list with the sentinel value
    gFallbackFonts[fallbackCount] = 0;
}
    ac.release();           // release the mutex now


}

///////////////////////////////////////////////////////////////////////////////

void SkFontHost::Serialize(const SkTypeface* face, SkWStream* stream) {
    const char* name = ((FamilyTypeface*)face)->getUniqueString();

    stream->write8((uint8_t)face->style());

    if (NULL == name || 0 == *name) {
        stream->writePackedUInt(0);
//        SkDebugf("--- fonthost serialize null\n");
    } else {
        uint32_t len = strlen(name);
        stream->writePackedUInt(len);
        stream->write(name, len);
//      SkDebugf("--- fonthost serialize <%s> %d\n", name, face->style());
    }
}

SkTypeface* SkFontHost::Deserialize(SkStream* stream) {
    load_system_fonts();

    int style = stream->readU8();

    int len = stream->readPackedUInt();
    if (len > 0) {
        SkString str;
        str.resize(len);
        stream->read(str.writable_str(), len);
        
        const FontInitRec* rec = gSystemFonts;
        for (size_t i = 0; i < SK_ARRAY_COUNT(gSystemFonts); i++) {
            if (strcmp(rec[i].fFileName, str.c_str()) == 0) {
                // backup until we hit the fNames
                for (int j = i; j >= 0; --j) {
                    if (rec[j].fNames != NULL) {
                        return SkFontHost::CreateTypeface(NULL,
                                    rec[j].fNames[0], (SkTypeface::Style)style);
                    }
                }
            }
        }
    }
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                       const char familyName[],
                                       SkTypeface::Style style) {
    load_system_fonts();

    SkAutoMutexAcquire  ac(gFamilyMutex);
    
    // clip to legal style bits
    style = (SkTypeface::Style)(style & SkTypeface::kBoldItalic);
    
    SkTypeface* tf = NULL;

    if (NULL != familyFace) {
        tf = find_typeface(familyFace, style);
    } else if (NULL != familyName) {
//        SkDebugf("======= familyName <%s>\n", familyName);
        tf = find_typeface(familyName, style);
    }

    if (NULL == tf) {
        tf = find_best_face(gDefaultFamily, style);
    }

    // we ref(), since the symantic is to return a new instance
    tf->ref();
    return tf;
}

bool SkFontHost::ValidFontID(uint32_t fontID) {
    SkAutoMutexAcquire  ac(gFamilyMutex);
    
    return find_from_uniqueID(fontID) != NULL;
}

SkStream* SkFontHost::OpenStream(uint32_t fontID) {
    SkAutoMutexAcquire  ac(gFamilyMutex);
    
    FamilyTypeface* tf = (FamilyTypeface*)find_from_uniqueID(fontID);
    SkStream* stream = tf ? tf->openStream() : NULL;

    if (stream && stream->getLength() == 0) {
        stream->unref();
        stream = NULL;
    }
    return stream;
}

size_t SkFontHost::GetFileName(SkFontID fontID, char path[], size_t length,
                               int32_t* index) {
    SkAutoMutexAcquire  ac(gFamilyMutex);

    FamilyTypeface* tf = (FamilyTypeface*)find_from_uniqueID(fontID);
    const char* src = tf ? tf->getFilePath() : NULL;

    if (src) {
        size_t size = strlen(src);
        if (path) {
            memcpy(path, src, SkMin32(size, length));
        }
        if (index) {
            *index = 0; // we don't have collections (yet)
        }
        return size;
    } else {
        return 0;
    }
}

uint32_t SkFontHost::NextLogicalFont(uint32_t fontID) {
    load_system_fonts();

    /*  First see if fontID is already one of our fallbacks. If so, return
        its successor. If fontID is not in our list, then return the first one
        in our list. Note: list is zero-terminated, and returning zero means
        we have no more fonts to use for fallbacks.
     */
    const uint32_t* list = gFallbackFonts;
    for (int i = 0; list[i] != 0; i++) {
        if (list[i] == fontID) {
            return list[i+1];
        }
    }
    return list[0];
}

///////////////////////////////////////////////////////////////////////////////

SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream* stream) {
    if (NULL == stream || stream->getLength() <= 0) {
        return NULL;
    }
    
    SkString name;
    SkTypeface::Style style;

    if (find_name_and_style(stream, &name, &style)) {
        return SkNEW_ARGS(StreamTypeface, (style, false, NULL, stream));
    } else {
        return NULL;
    }
}

/* SAMSUNG CHANGE >> */    
/** Returns true if the given stream is a valid typeface stream
*/
bool SkFontHost::isValidStream(SkStream* stream) {
    return is_valid_stream(stream);
}
/* SAMSUNG CHANGE << */    

SkTypeface* SkFontHost::CreateTypefaceFromFile(const char path[]) {
    SkStream* stream = SkNEW_ARGS(SkMMAPStream, (path));
    SkTypeface* face = SkFontHost::CreateTypefaceFromStream(stream);
    // since we created the stream, we let go of our ref() here
    stream->unref();
    return face;
}

///////////////////////////////////////////////////////////////////////////////

size_t SkFontHost::ShouldPurgeFontCache(size_t sizeAllocatedSoFar) {
    if (sizeAllocatedSoFar > FONT_CACHE_MEMORY_BUDGET)
        return sizeAllocatedSoFar - FONT_CACHE_MEMORY_BUDGET;
    else
        return 0;   // nothing to do
}

#ifdef MONOTYPE_FLIPFONT
void SkFontHost::PrepareFontFlip() {

    SkAutoMutexAcquire    ac(ffMutex);
    int i;

    SkGlyphCache::SetCacheUsed(0);
    if(oldSansUID == 0)
        SkFontHost::FetchSansUniqueIDs(&oldSansUID, &oldSansBoldUID);


    // Collect the current bold face if it's unique. Later we'll set all former bold faces to reflect the boldness of
    //     the active font. This is 'cause some apps, such as the webkit apps, hold on to their
    //     old faces and use them to decide if pseudo-bold should be applied, and in froyo we can no longer
    //     kill the running apps.
 
    SkTypeface *currentSansBoldTypeface = find_best_face(gDefaultFamily, SkTypeface::kBold);   
    for(i=0; i<NUM_OLD_BOLD_FACES;i++) {
        if(oldSansBoldTypefaces[i] == currentSansBoldTypeface) { // got this one already
            __android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "duplicate face ***********");
            break;
        }
        if(oldSansBoldTypefaces[i] == NULL) {
            oldSansBoldTypefaces[i] = currentSansBoldTypeface; // got a new one!
            __android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "stored typeface in slot %d", i);
            break;
        }
    }
    if(i >= NUM_OLD_BOLD_FACES)
        __android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "exceeded NUM_OLD_BOLD_FACES ***********");

    
    gDefaultNormal = NULL;
    
    load_system_fonts();
                
    SkTypeface *newSansBoldTypeface   = find_best_face(gDefaultFamily, SkTypeface::kBold);   
    
    for(i=0; i<NUM_OLD_BOLD_FACES;i++) {
        if(oldSansBoldTypefaces[i] != NULL)
            oldSansBoldTypefaces[i]->setBold(newSansBoldTypeface->isBold());
        else
            break;
    }

    gFontHasJustBeenFlipped = true;  
    ac.release();           // release the mutex now
  

}



void SkFontHost::FetchSansUniqueIDs(uint32_t *sans, uint32_t *sansBold) {

    

    SkTypeface *defaultNormal = find_best_face(gDefaultFamily, SkTypeface::kNormal);

    SkTypeface *defaultBold   = find_best_face(gDefaultFamily, SkTypeface::kBold);

 

   *sans     = defaultNormal->uniqueID();

   *sansBold = defaultBold->uniqueID();

}


int getCopyrightAndName(char *buffer, char **copyright, char **name)

{

    int i;
    int numtables;
    char *textname;
    long nametag;

    long tag;
    long checksum;
    long offset;
    long size;
      
    short platform;
    short encoding;
    short language;
    short nameid;
    short strlen;
    short stroffset;
    short format;
    short numrecords;
    short nameoffset;

    char *ptr,*p;
    short *sptr;
    long *lptr;
    int found;

    /* get number of tables */
    sptr = (short *)(buffer + 4);
    numtables = *sptr;
    numtables = SWAPWORD(numtables);
      
    textname = "name";
    nametag = *(long *)textname;

    lptr = (long *)(buffer + 12);

    for(i=0; i<numtables; i++) {
        tag = *lptr++;
        checksum = *lptr++;
        offset = *lptr++;
        size = *lptr++;
      
        if(tag == nametag) {
            offset = SWAPLONG(offset);
            size = SWAPLONG(size);
            break;
        } 
    }

    /******* first get copyright ***********/
	sptr = (short *)(buffer + offset);
    format = *sptr++;
    numrecords = *sptr++;
    stroffset = *sptr++;

    numrecords = SWAPWORD(numrecords);
    stroffset = SWAPWORD(stroffset);
  
    found = 0;

    for(i=0; i<numrecords; i++) {
        platform = *sptr++;
        encoding = *sptr++;
        language = *sptr++;
        nameid = *sptr++;
        strlen = *sptr++;
        nameoffset = *sptr++;
        platform = SWAPWORD(platform);
        nameid = SWAPWORD(nameid);

        /* platform 1 is the Mac table. If you use platform 3, you must remove the spaces
        from the name - see commented out code below. */

        if(platform == 1 && nameid == 0) {
        found = 1;
        break;
        }
        if(platform == 3) {
        found = 1;
        break;
        }  
    }


    if(found) {

        strlen = SWAPWORD(strlen);
        nameoffset = SWAPWORD(nameoffset);
        ptr = buffer + offset + stroffset + nameoffset;
        *copyright = (char*)malloc(strlen+1);
  
        memcpy(*copyright,ptr,strlen);
        (*copyright)[strlen] = 0;
        
        if(platform == 3) { 
            ptr = *copyright;
            p = ptr;

            /*  remove zeros from name */           

            for(i=0; i<strlen; i++) {
                if(*p != 0) {
                    *ptr = *p;
                    ptr++;
                    }
                p++;
                }
            *ptr = 0;
        }
    }
        
    else {
        (*copyright)[0] = 0;
        }   
	/********** then get name *************/

	sptr = (short *)(buffer + offset);
    format = *sptr++;
    numrecords = *sptr++;
    stroffset = *sptr++;

    numrecords = SWAPWORD(numrecords);
    stroffset = SWAPWORD(stroffset);
  
    found = 0;

    for(i=0; i<numrecords; i++) {
        platform = *sptr++;
        encoding = *sptr++;
        language = *sptr++;
        nameid = *sptr++;
        strlen = *sptr++;
        nameoffset = *sptr++;
        platform = SWAPWORD(platform);
        nameid = SWAPWORD(nameid);

        /* platform 1 is the Mac table. If you use platform 3, you must remove the spaces

        from the name - see commented out code below. */

        if(platform == 1 && nameid == 1) {
        found = 1;
        break;
        }

        if(platform == 3 && nameid == 1) {
        found = 1;
        break;
        }  
    }


    if(found) {
        strlen = SWAPWORD(strlen);
        nameoffset = SWAPWORD(nameoffset);
        ptr = buffer + offset + stroffset + nameoffset;
        *name = (char*)malloc(strlen+1);
  
        memcpy(*name,ptr,strlen);
        (*name)[strlen] = 0;
        
        if(platform == 3) { 
            ptr = *name;
            p = ptr;

            /*  remove zeros from name */           

            for(i=0; i<strlen; i++) {
                if(*p != 0) {
                    *ptr = *p;
                    ptr++;
                    }
                p++;
                }
            *ptr = 0;
        }
    }
    else {
        (*name)[0] = 0;
        }   
    return 0;
}

bool CopyrightAndNameAreValid(FILE *fp)
{
    // get copyright string
    char *copyright;
    char *name;
	int filelen;
	char *buffer;
	bool retVal = false;

	fseek(fp,0,SEEK_END);
	filelen = ftell(fp);
	if(filelen) {
	fseek(fp,0,SEEK_SET);
	buffer = (char*)malloc(filelen);
	fread(buffer,filelen,1,fp);
	int status = getCopyrightAndName(buffer,&copyright, &name);
	free(buffer);
	                                    
	// see if copyright is Monotype font
	char str[256];
	size_t len = strlen(copyright);
	size_t len1 = strlen(MONOTYPE_COPYRIGHT);
	
	// if copyright string too short
	if((len-len1) <= 0){
	    free(copyright);
	    free(name);
	    return false;
	    }
	strcpy(str, copyright+(len-len1));
	
		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Copyright string - %s", copyright);
		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Copyright 2 string - %s", str);
		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Name string - %s", name);
		
    bool bMonotypeFont = false;
	bool bGoodName = false;   

    if (strcmp(str,MONOTYPE_COPYRIGHT) == 0){
			//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "MonoType copyright string matches");
        bMonotypeFont = true;
        }
    else {   							 	
			//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "MonoType copyright string doesn't match");
        bMonotypeFont = false;
        }
        
    // see if copyright is Samsung font
    len = strlen(copyright);
	len1 = strlen(YOON_COPYRIGHT);
    
    // if copyright string too short
	if((len-len1) <= 0){
	    free(copyright);
	    free(name);
	    return false;
	    }	    
    strcpy(str, copyright+(len-len1));

		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Copyright string - %s", copyright);
		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Copyright 2 string - %s", str);
		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Name string - %s", name);

    bool bSamsungFont = false;    	    
		if (strcmp(str,YOON_COPYRIGHT) == 0) {
			//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Samsung copyright string matches");
        bSamsungFont = true;
			/* well, now let's check the samsung name */
			if (strcmp(name,YOON_FONTNAME_1) == 0 || strcmp(name,YOON_FONTNAME_2)==0 ||
				strcmp(name,YOON_FONTNAME_3) == 0 || strcmp(name,YOON_FONTNAME_4)==0 ||
				strcmp(name,YOON_FONTNAME_5) == 0 || strcmp(name,YOON_FONTNAME_6)==0 ||
				strcmp(name,YOON_FONTNAME_7) == 0) 
				bGoodName = true;
		}
		else {   							 	
			//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Samsung copyright string doesn't match");
        bSamsungFont = false;
        }
	    
	free(copyright);    
    free(name);

		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Copyright is freed");

		retVal = (bGoodName && bSamsungFont) || bMonotypeFont;
		//__android_log_print(ANDROID_LOG_DEBUG, "SKIA_FONT", "Exiting CopyrightAndNameAreValid - retVal = %d", retVal);

	}
	return retVal;
}

#endif //MONOTYPE_FLIPFONT

