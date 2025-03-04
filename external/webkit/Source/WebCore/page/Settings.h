/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Settings_h
#define Settings_h

#include "EditingBehaviorTypes.h"
#include "FontRenderingMode.h"
#include "KURL.h"
#include <wtf/text/AtomicString.h>
#include "Color.h" // SAMSUNG CHANGE : ADVANCED COPY & PASTE

namespace WebCore {

    class Page;

    enum EditableLinkBehavior {
        EditableLinkDefaultBehavior,
        EditableLinkAlwaysLive,
        EditableLinkOnlyLiveWithShiftKey,
        EditableLinkLiveWhenNotFocused,
        EditableLinkNeverLive
    };

    enum TextDirectionSubmenuInclusionBehavior {
        TextDirectionSubmenuNeverIncluded,
        TextDirectionSubmenuAutomaticallyIncluded,
        TextDirectionSubmenuAlwaysIncluded
    };

    class Settings {
        WTF_MAKE_NONCOPYABLE(Settings); WTF_MAKE_FAST_ALLOCATED;
    public:
        Settings(Page*);

#ifdef ANDROID_LAYOUT
        // FIXME: How do we determine the margins other than guessing?
        #define ANDROID_SSR_MARGIN_PADDING  3
        #define ANDROID_FCTS_MARGIN_PADDING  10
#endif
        enum LayoutAlgorithm {
            kLayoutNormal,
            kLayoutSSR,
            kLayoutFitColumnToScreen
        };
#if ENABLE(SPELLCHECK)
	// SAMSUNG CHANGES +
	//Written this functions for spellcheck feature 
	void setIsContinousSpellCheck(bool bisspellcheckenabled);
	bool getIsContinousSpellCheck(){return m_isspellcheckenabled;}
	// SAMSUNG CHANGES -
#endif
        void setStandardFontFamily(const AtomicString&);
        const AtomicString& standardFontFamily() const { return m_standardFontFamily; }

        void setFixedFontFamily(const AtomicString&);
        const AtomicString& fixedFontFamily() const { return m_fixedFontFamily; }

#ifdef ANDROID_LAYOUT
        LayoutAlgorithm layoutAlgorithm() const { return m_layoutAlgorithm; }
        void setLayoutAlgorithm(LayoutAlgorithm algorithm) { m_layoutAlgorithm = algorithm; }

        bool useWideViewport() const { return m_useWideViewport; }
        void setUseWideViewport(bool use) { m_useWideViewport = use; }
#endif

        void setSerifFontFamily(const AtomicString&);
        const AtomicString& serifFontFamily() const { return m_serifFontFamily; }

        void setSansSerifFontFamily(const AtomicString&);
        const AtomicString& sansSerifFontFamily() const { return m_sansSerifFontFamily; }

        void setCursiveFontFamily(const AtomicString&);
        const AtomicString& cursiveFontFamily() const { return m_cursiveFontFamily; }

        void setFantasyFontFamily(const AtomicString&);
        const AtomicString& fantasyFontFamily() const { return m_fantasyFontFamily; }

        void setMinimumFontSize(int);
        int minimumFontSize() const { return m_minimumFontSize; }

        void setMinimumLogicalFontSize(int);
        int minimumLogicalFontSize() const { return m_minimumLogicalFontSize; }

        void setDefaultFontSize(int);
        int defaultFontSize() const { return m_defaultFontSize; }

        void setDefaultFixedFontSize(int);
        int defaultFixedFontSize() const { return m_defaultFixedFontSize; }

        // Unlike areImagesEnabled, this only suppresses the network load of
        // the image URL.  A cached image will still be rendered if requested.
        void setLoadsImagesAutomatically(bool);
        bool loadsImagesAutomatically() const { return m_loadsImagesAutomatically; }

        // This setting only affects site icon image loading if loadsImagesAutomatically setting is false and this setting is true.
        // All other permutations still heed loadsImagesAutomatically setting.
        void setLoadsSiteIconsIgnoringImageLoadingSetting(bool);
        bool loadsSiteIconsIgnoringImageLoadingSetting() const { return m_loadsSiteIconsIgnoringImageLoadingSetting; }

        void setJavaScriptEnabled(bool);
        // Instead of calling isJavaScriptEnabled directly, please consider calling
        // ScriptController::canExecuteScripts, which takes things like the
        // HTML sandbox attribute into account.
        bool isJavaScriptEnabled() const { return m_isJavaScriptEnabled; }

        void setWebSecurityEnabled(bool);
        bool isWebSecurityEnabled() const { return m_isWebSecurityEnabled; }

        void setAllowUniversalAccessFromFileURLs(bool);
        bool allowUniversalAccessFromFileURLs() const { return m_allowUniversalAccessFromFileURLs; }

        void setAllowFileAccessFromFileURLs(bool);
        bool allowFileAccessFromFileURLs() const { return m_allowFileAccessFromFileURLs; }

        void setJavaScriptCanOpenWindowsAutomatically(bool);
        bool javaScriptCanOpenWindowsAutomatically() const { return m_javaScriptCanOpenWindowsAutomatically; }

        void setJavaScriptCanAccessClipboard(bool);
        bool javaScriptCanAccessClipboard() const { return m_javaScriptCanAccessClipboard; }

        void setSpatialNavigationEnabled(bool);
        bool isSpatialNavigationEnabled() const { return m_isSpatialNavigationEnabled; }

        void setJavaEnabled(bool);
        bool isJavaEnabled() const { return m_isJavaEnabled; }

        void setImagesEnabled(bool);
        bool areImagesEnabled() const { return m_areImagesEnabled; }

        void setMediaEnabled(bool);
        bool isMediaEnabled() const { return m_isMediaEnabled; }

        void setPluginsEnabled(bool);
        bool arePluginsEnabled() const { return m_arePluginsEnabled; }

#ifdef ANDROID_PLUGINS
        void setPluginsOnDemand(bool onDemand) { m_pluginsOnDemand = onDemand; }
        bool arePluginsOnDemand() const { return m_pluginsOnDemand; }
#endif

        void setLocalStorageEnabled(bool);
        bool localStorageEnabled() const { return m_localStorageEnabled; }

#if ENABLE(DOM_STORAGE)
        // Allow clients concerned with memory consumption to set a quota on session storage
        // since the memory used won't be released until the Page is destroyed.
        // Default is noQuota.
        void setSessionStorageQuota(unsigned);
        unsigned sessionStorageQuota() const { return m_sessionStorageQuota; }
#endif

        // When this option is set, WebCore will avoid storing any record of browsing activity
        // that may persist on disk or remain displayed when the option is reset.
        // This option does not affect the storage of such information in RAM.
        // The following functions respect this setting:
        //  - HTML5/DOM Storage
        //  - Icon Database
        //  - Console Messages
        //  - MemoryCache
        //  - Application Cache
        //  - Back/Forward Page History
        //  - Page Search Results
        //  - HTTP Cookies
        //  - Plug-ins (that support NPNVprivateModeBool)
        void setPrivateBrowsingEnabled(bool);
        bool privateBrowsingEnabled() const { return m_privateBrowsingEnabled; }

        void setCaretBrowsingEnabled(bool);
        bool caretBrowsingEnabled() const { return m_caretBrowsingEnabled; }

        void setDefaultTextEncodingName(const String&);
        const String& defaultTextEncodingName() const { return m_defaultTextEncodingName; }
        
        void setUsesEncodingDetector(bool);
        bool usesEncodingDetector() const { return m_usesEncodingDetector; }

        void setDNSPrefetchingEnabled(bool);
        bool dnsPrefetchingEnabled() const { return m_dnsPrefetchingEnabled; }

        void setUserStyleSheetLocation(const KURL&);
        const KURL& userStyleSheetLocation() const { return m_userStyleSheetLocation; }

        void setShouldPrintBackgrounds(bool);
        bool shouldPrintBackgrounds() const { return m_shouldPrintBackgrounds; }

        void setTextAreasAreResizable(bool);
        bool textAreasAreResizable() const { return m_textAreasAreResizable; }

        void setEditableLinkBehavior(EditableLinkBehavior);
        EditableLinkBehavior editableLinkBehavior() const { return m_editableLinkBehavior; }

        void setTextDirectionSubmenuInclusionBehavior(TextDirectionSubmenuInclusionBehavior);
        TextDirectionSubmenuInclusionBehavior textDirectionSubmenuInclusionBehavior() const { return m_textDirectionSubmenuInclusionBehavior; }

#if ENABLE(DASHBOARD_SUPPORT)
        void setUsesDashboardBackwardCompatibilityMode(bool);
        bool usesDashboardBackwardCompatibilityMode() const { return m_usesDashboardBackwardCompatibilityMode; }
#endif
        
        void setNeedsAdobeFrameReloadingQuirk(bool);
        bool needsAcrobatFrameReloadingQuirk() const { return m_needsAdobeFrameReloadingQuirk; }

        void setNeedsKeyboardEventDisambiguationQuirks(bool);
        bool needsKeyboardEventDisambiguationQuirks() const { return m_needsKeyboardEventDisambiguationQuirks; }

        void setTreatsAnyTextCSSLinkAsStylesheet(bool);
        bool treatsAnyTextCSSLinkAsStylesheet() const { return m_treatsAnyTextCSSLinkAsStylesheet; }

        void setNeedsLeopardMailQuirks(bool);
        bool needsLeopardMailQuirks() const { return m_needsLeopardMailQuirks; }

        void setNeedsTigerMailQuirks(bool);
        bool needsTigerMailQuirks() const { return m_needsTigerMailQuirks; }

        void setDOMPasteAllowed(bool);
        bool isDOMPasteAllowed() const { return m_isDOMPasteAllowed; }
        
        static void setDefaultMinDOMTimerInterval(double); // Interval specified in seconds.
        static double defaultMinDOMTimerInterval();
        
        void setMinDOMTimerInterval(double); // Per-page; initialized to default value.
        double minDOMTimerInterval();

        void setUsesPageCache(bool);
        bool usesPageCache() const { return m_usesPageCache; }

        void setShrinksStandaloneImagesToFit(bool);
        bool shrinksStandaloneImagesToFit() const { return m_shrinksStandaloneImagesToFit; }

        void setShowsURLsInToolTips(bool);
        bool showsURLsInToolTips() const { return m_showsURLsInToolTips; }

        void setFTPDirectoryTemplatePath(const String&);
        const String& ftpDirectoryTemplatePath() const { return m_ftpDirectoryTemplatePath; }
        
        void setForceFTPDirectoryListings(bool);
        bool forceFTPDirectoryListings() const { return m_forceFTPDirectoryListings; }
        
        void setDeveloperExtrasEnabled(bool);
        bool developerExtrasEnabled() const { return m_developerExtrasEnabled; }

        void setFrameFlatteningEnabled(bool);
        bool frameFlatteningEnabled() const { return m_frameFlatteningEnabled; }

#ifdef ANDROID_META_SUPPORT
        void resetMetadataSettings();
        void setMetadataSettings(const String& key, const String& value);

        void setViewportWidth(int);
        int viewportWidth() const { return m_viewport_width; }

        void setViewportHeight(int);
        int viewportHeight() const { return m_viewport_height; }

        void setViewportInitialScale(int);
        int viewportInitialScale() const { return m_viewport_initial_scale; }

        void setViewportMinimumScale(int);
        int viewportMinimumScale() const { return m_viewport_minimum_scale; }

        void setViewportMaximumScale(int);
        int viewportMaximumScale() const { return m_viewport_maximum_scale; }

        void setViewportUserScalable(bool);
        bool viewportUserScalable() const { return m_viewport_user_scalable; }

        void setViewportTargetDensityDpi(int);
        int viewportTargetDensityDpi() const { return m_viewport_target_densitydpi; }

        void setFormatDetectionAddress(bool);
        bool formatDetectionAddress() const { return m_format_detection_address; }

        void setFormatDetectionEmail(bool);
        bool formatDetectionEmail() const { return m_format_detection_email; }

        void setFormatDetectionTelephone(bool);
        bool formatDetectionTelephone() const { return m_format_detection_telephone; }

        void setDefaultFormatDetection(bool flag) { m_default_format_detection = flag; }
#endif
#ifdef ANDROID_MULTIPLE_WINDOWS
        bool supportMultipleWindows() const { return m_supportMultipleWindows; }
        void setSupportMultipleWindows(bool support) { m_supportMultipleWindows = support; }
#endif
        void setAuthorAndUserStylesEnabled(bool);
        bool authorAndUserStylesEnabled() const { return m_authorAndUserStylesEnabled; }
        
        void setFontRenderingMode(FontRenderingMode mode);
        FontRenderingMode fontRenderingMode() const;

        void setNeedsSiteSpecificQuirks(bool);
        bool needsSiteSpecificQuirks() const { return m_needsSiteSpecificQuirks; }

#if ENABLE(WEB_ARCHIVE)
        void setWebArchiveDebugModeEnabled(bool);
        bool webArchiveDebugModeEnabled() const { return m_webArchiveDebugModeEnabled; }
#endif

        void setLocalFileContentSniffingEnabled(bool);
        bool localFileContentSniffingEnabled() const { return m_localFileContentSniffingEnabled; }

        void setLocalStorageDatabasePath(const String&);
        const String& localStorageDatabasePath() const { return m_localStorageDatabasePath; }

        void setApplicationChromeMode(bool);
        bool inApplicationChromeMode() const { return m_inApplicationChromeMode; }

        void setOfflineWebApplicationCacheEnabled(bool);
        bool offlineWebApplicationCacheEnabled() const { return m_offlineWebApplicationCacheEnabled; }

        void setShouldPaintCustomScrollbars(bool);
        bool shouldPaintCustomScrollbars() const { return m_shouldPaintCustomScrollbars; }
        
        void setEnforceCSSMIMETypeInNoQuirksMode(bool);
        bool enforceCSSMIMETypeInNoQuirksMode() { return m_enforceCSSMIMETypeInNoQuirksMode; }

        void setMaximumDecodedImageSize(size_t size) { m_maximumDecodedImageSize = size; }
        size_t maximumDecodedImageSize() const { return m_maximumDecodedImageSize; }

#if USE(SAFARI_THEME)
        // Windows debugging pref (global) for switching between the Aqua look and a native windows look.
        static void setShouldPaintNativeControls(bool);
        static bool shouldPaintNativeControls() { return gShouldPaintNativeControls; }
#endif

        void setAllowScriptsToCloseWindows(bool);
        bool allowScriptsToCloseWindows() const { return m_allowScriptsToCloseWindows; }

        void setEditingBehaviorType(EditingBehaviorType behavior) { m_editingBehaviorType = behavior; }
        EditingBehaviorType editingBehaviorType() const { return static_cast<EditingBehaviorType>(m_editingBehaviorType); }

        void setDownloadableBinaryFontsEnabled(bool);
        bool downloadableBinaryFontsEnabled() const { return m_downloadableBinaryFontsEnabled; }

        void setXSSAuditorEnabled(bool);
        bool xssAuditorEnabled() const { return m_xssAuditorEnabled; }

        void setCanvasUsesAcceleratedDrawing(bool);
        bool canvasUsesAcceleratedDrawing() const { return m_canvasUsesAcceleratedDrawing; }

        void setAcceleratedDrawingEnabled(bool);
        bool acceleratedDrawingEnabled() const { return m_acceleratedDrawingEnabled; }
        
        void setAcceleratedCompositingEnabled(bool);
        bool acceleratedCompositingEnabled() const { return m_acceleratedCompositingEnabled; }

        void setAcceleratedCompositingFor3DTransformsEnabled(bool);
        bool acceleratedCompositingFor3DTransformsEnabled() const { return m_acceleratedCompositingFor3DTransformsEnabled; }

        void setAcceleratedCompositingForVideoEnabled(bool);
        bool acceleratedCompositingForVideoEnabled() const { return m_acceleratedCompositingForVideoEnabled; }

        void setAcceleratedCompositingForPluginsEnabled(bool);
        bool acceleratedCompositingForPluginsEnabled() const { return m_acceleratedCompositingForPluginsEnabled; }

        void setAcceleratedCompositingForCanvasEnabled(bool);
        bool acceleratedCompositingForCanvasEnabled() const { return m_acceleratedCompositingForCanvasEnabled; }

        void setAcceleratedCompositingForAnimationEnabled(bool);
        bool acceleratedCompositingForAnimationEnabled() const { return m_acceleratedCompositingForAnimationEnabled; }

        void setShowDebugBorders(bool);
        bool showDebugBorders() const { return m_showDebugBorders; }

        void setShowRepaintCounter(bool);
        bool showRepaintCounter() const { return m_showRepaintCounter; }

        void setExperimentalNotificationsEnabled(bool);
        bool experimentalNotificationsEnabled() const { return m_experimentalNotificationsEnabled; }

#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
        static void setShouldUseHighResolutionTimers(bool);
        static bool shouldUseHighResolutionTimers() { return gShouldUseHighResolutionTimers; }
#endif

        void setPluginAllowedRunTime(unsigned);
        unsigned pluginAllowedRunTime() const { return m_pluginAllowedRunTime; }

        void setWebAudioEnabled(bool);
        bool webAudioEnabled() const { return m_webAudioEnabled; }

        void setWebGLEnabled(bool);
        bool webGLEnabled() const { return m_webGLEnabled; }

        void setOpenGLMultisamplingEnabled(bool);
        bool openGLMultisamplingEnabled() const { return m_openGLMultisamplingEnabled; }

        void setAccelerated2dCanvasEnabled(bool);
        bool accelerated2dCanvasEnabled() const { return m_acceleratedCanvas2dEnabled; }

        void setLoadDeferringEnabled(bool);
        bool loadDeferringEnabled() const { return m_loadDeferringEnabled; }
        
		//SAMSUNG_CHANGES >>		
		void setRssSniffingEnabled(bool enabled);        
		bool rssSniffingEnabled() const { return m_rssSniffingnEnabled; }
		//SAMSUNG_CHANGES <<
        void setTiledBackingStoreEnabled(bool);
        bool tiledBackingStoreEnabled() const { return m_tiledBackingStoreEnabled; }

        //SAMSUNG CHANGE ADVANCED TEXT SELECTION +
        void setAdvancedSelectionEnabled(bool);
        bool advancedSelectionEnabled() const { return m_advancedSelectionEnabled; }
        void setAdvancedSelectionBgColor(int, int, int);
        Color advancedSelectionBgColor() const { return m_advancedSelectionBgColor; }
        //SAMSUNG CHANGE ADVANCED TEXT SELECTION -
        
        void setPaginateDuringLayoutEnabled(bool flag) { m_paginateDuringLayoutEnabled = flag; }
        bool paginateDuringLayoutEnabled() const { return m_paginateDuringLayoutEnabled; }

#if ENABLE(FULLSCREEN_API)
        void setFullScreenEnabled(bool flag) { m_fullScreenAPIEnabled = flag; }
        bool fullScreenEnabled() const  { return m_fullScreenAPIEnabled; }
#endif

        void setAsynchronousSpellCheckingEnabled(bool flag) { m_asynchronousSpellCheckingEnabled = flag; }
        bool asynchronousSpellCheckingEnabled() const  { return m_asynchronousSpellCheckingEnabled; }

        void setMemoryInfoEnabled(bool flag) { m_memoryInfoEnabled = flag; }
        bool memoryInfoEnabled() const { return m_memoryInfoEnabled; }

        // This setting will be removed when an HTML5 compatibility issue is
        // resolved and WebKit implementation of interactive validation is
        // completed. See http://webkit.org/b/40520, http://webkit.org/b/40747,
        // and http://webkit.org/b/40908
        void setInteractiveFormValidationEnabled(bool flag) { m_interactiveFormValidation = flag; }
        bool interactiveFormValidationEnabled() const { return m_interactiveFormValidation; }

        // Sets the maginication value for validation message timer.
        // If the maginication value is N, a validation message disappears
        // automatically after <message length> * N / 1000 seconds. If N is
        // equal to or less than 0, a validation message doesn't disappears
        // automaticaly. The default value is 50.
        void setValidationMessageTimerMagnification(int newValue) { m_validationMessageTimerMagnification = newValue; }
        int validationMessageTimerMaginification() const { return m_validationMessageTimerMagnification; }
        
        void setUsePreHTML5ParserQuirks(bool flag) { m_usePreHTML5ParserQuirks = flag; }
        bool usePreHTML5ParserQuirks() const { return m_usePreHTML5ParserQuirks; }

        void setHyperlinkAuditingEnabled(bool flag) { m_hyperlinkAuditingEnabled = flag; }
        bool hyperlinkAuditingEnabled() const { return m_hyperlinkAuditingEnabled; }

        void setCrossOriginCheckInGetMatchedCSSRulesDisabled(bool flag) { m_crossOriginCheckInGetMatchedCSSRulesDisabled = flag; }
        bool crossOriginCheckInGetMatchedCSSRulesDisabled() const { return m_crossOriginCheckInGetMatchedCSSRulesDisabled; }
        
        void setUseQuickLookResourceCachingQuirks(bool flag) { m_useQuickLookResourceCachingQuirks = flag; }
        bool useQuickLookResourceCachingQuirks() const { return m_useQuickLookResourceCachingQuirks; }

        void setForceCompositingMode(bool flag) { m_forceCompositingMode = flag; }
        bool forceCompositingMode() { return m_forceCompositingMode; }
        
        void setShouldInjectUserScriptsInInitialEmptyDocument(bool flag) { m_shouldInjectUserScriptsInInitialEmptyDocument = flag; }
        bool shouldInjectUserScriptsInInitialEmptyDocument() { return m_shouldInjectUserScriptsInInitialEmptyDocument; }

#if ENABLE(WEB_AUTOFILL)
        void setAutoFillEnabled(bool flag) { m_autoFillEnabled = flag; }
        bool autoFillEnabled() { return m_autoFillEnabled; }
#endif
	//SISO_HTMLCOMPOSER begin 
	void setEditableSupportEnabled(bool);
	bool editableSupportEnabled() const { return m_editableSupportEnabled; }
	//SISO_HTMLCOMPOSER end

#ifdef ANDROID_BLOCK_NETWORK_IMAGE
        void setBlockNetworkImage(bool);
        bool blockNetworkImage() const { return m_blockNetworkImage; }
#endif

#ifdef WEBKIT_TEXT_SIZE_ADJUST
//SAMSUNG CHANGE :TEXT READABILITY >>
        void EnableTextReadability(bool);
        bool TextReadability() const { return m_TextReadability; }
 	    void setTextReadabilityRatio(float);
        float TextReadabilityRatio() const { return m_TextReadabilityRatio; }
 	//SAMSUNG CHANGE :TEXT READABILITY <<
#endif

//SISO WOFF CHANGES <<
	void setWOFFEnabled(bool);
        bool woffEnabled() const { return m_woffEnabled; }
//SISO WOFF CHANGES >>

    private:
        Page* m_page;

        String m_defaultTextEncodingName;
        String m_ftpDirectoryTemplatePath;
        String m_localStorageDatabasePath;
        KURL m_userStyleSheetLocation;
        AtomicString m_standardFontFamily;
        AtomicString m_fixedFontFamily;
        AtomicString m_serifFontFamily;
        AtomicString m_sansSerifFontFamily;
        AtomicString m_cursiveFontFamily;
        AtomicString m_fantasyFontFamily;
        EditableLinkBehavior m_editableLinkBehavior;
        TextDirectionSubmenuInclusionBehavior m_textDirectionSubmenuInclusionBehavior;
        int m_minimumFontSize;
        int m_minimumLogicalFontSize;
        int m_defaultFontSize;
        int m_defaultFixedFontSize;
        int m_validationMessageTimerMagnification;
        size_t m_maximumDecodedImageSize;
#if ENABLE(DOM_STORAGE)
        unsigned m_sessionStorageQuota;
#endif
        unsigned m_pluginAllowedRunTime;
        unsigned m_editingBehaviorType;
#ifdef ANDROID_META_SUPPORT
        // range is from 200 to 10,000. 0 is a special value means device-width.
        // default is -1, which means undefined.
        int m_viewport_width;
        // range is from 223 to 10,000. 0 is a special value means device-height
        // default is -1, which means undefined.
        int m_viewport_height;
        // range is from 1 to 1000 in percent. default is 0, which means undefined.
        int m_viewport_initial_scale;
        // range is from 1 to 1000 in percent. default is 0, which means undefined.
        int m_viewport_minimum_scale;
        // range is from 1 to 1000 in percent. default is 0, which means undefined.
        int m_viewport_maximum_scale;
#endif
#ifdef ANDROID_LAYOUT
        LayoutAlgorithm m_layoutAlgorithm;
#endif
        bool m_isSpatialNavigationEnabled : 1;
        bool m_isJavaEnabled : 1;
        bool m_loadsImagesAutomatically : 1;
        bool m_loadsSiteIconsIgnoringImageLoadingSetting : 1;
        bool m_privateBrowsingEnabled : 1;
        bool m_caretBrowsingEnabled : 1;
        bool m_areImagesEnabled : 1;
        bool m_isMediaEnabled : 1;
        bool m_arePluginsEnabled : 1;
        bool m_localStorageEnabled : 1;
        bool m_isJavaScriptEnabled : 1;
        bool m_isWebSecurityEnabled : 1;
        bool m_allowUniversalAccessFromFileURLs: 1;
        bool m_allowFileAccessFromFileURLs: 1;
        bool m_javaScriptCanOpenWindowsAutomatically : 1;
        bool m_javaScriptCanAccessClipboard : 1;
        bool m_shouldPrintBackgrounds : 1;
        bool m_textAreasAreResizable : 1;
#if ENABLE(DASHBOARD_SUPPORT)
        bool m_usesDashboardBackwardCompatibilityMode : 1;
#endif
        bool m_needsAdobeFrameReloadingQuirk : 1;
        bool m_needsKeyboardEventDisambiguationQuirks : 1;
        bool m_treatsAnyTextCSSLinkAsStylesheet : 1;
        bool m_needsLeopardMailQuirks : 1;
        bool m_needsTigerMailQuirks : 1;
        bool m_isDOMPasteAllowed : 1;
        bool m_shrinksStandaloneImagesToFit : 1;
        bool m_usesPageCache: 1;
        bool m_showsURLsInToolTips : 1;
        bool m_forceFTPDirectoryListings : 1;
        bool m_developerExtrasEnabled : 1;
        bool m_authorAndUserStylesEnabled : 1;
        bool m_needsSiteSpecificQuirks : 1;
        unsigned m_fontRenderingMode : 1;
        bool m_frameFlatteningEnabled : 1;
        bool m_webArchiveDebugModeEnabled : 1;
        bool m_localFileContentSniffingEnabled : 1;
        bool m_inApplicationChromeMode : 1;
        bool m_offlineWebApplicationCacheEnabled : 1;
        bool m_shouldPaintCustomScrollbars : 1;
        bool m_enforceCSSMIMETypeInNoQuirksMode : 1;
        bool m_usesEncodingDetector : 1;
        bool m_allowScriptsToCloseWindows : 1;
        bool m_canvasUsesAcceleratedDrawing : 1;
        bool m_acceleratedDrawingEnabled : 1;
        bool m_downloadableBinaryFontsEnabled : 1;
        bool m_xssAuditorEnabled : 1;
        bool m_acceleratedCompositingEnabled : 1;
        bool m_acceleratedCompositingFor3DTransformsEnabled : 1;
        bool m_acceleratedCompositingForVideoEnabled : 1;
        bool m_acceleratedCompositingForPluginsEnabled : 1;
        bool m_acceleratedCompositingForCanvasEnabled : 1;
        bool m_acceleratedCompositingForAnimationEnabled : 1;
        bool m_showDebugBorders : 1;
        bool m_showRepaintCounter : 1;
        bool m_experimentalNotificationsEnabled : 1;
        bool m_webGLEnabled : 1;
        bool m_openGLMultisamplingEnabled : 1;
        bool m_webAudioEnabled : 1;
        bool m_acceleratedCanvas2dEnabled : 1;
        bool m_loadDeferringEnabled : 1;
        bool m_tiledBackingStoreEnabled : 1;
        bool m_paginateDuringLayoutEnabled : 1;
        bool m_dnsPrefetchingEnabled : 1;
#if ENABLE(FULLSCREEN_API)
        bool m_fullScreenAPIEnabled : 1;
#endif
        bool m_asynchronousSpellCheckingEnabled: 1;
        bool m_memoryInfoEnabled: 1;
        bool m_interactiveFormValidation: 1;
        bool m_usePreHTML5ParserQuirks: 1;
        bool m_hyperlinkAuditingEnabled : 1;
        bool m_crossOriginCheckInGetMatchedCSSRulesDisabled : 1;
        bool m_useQuickLookResourceCachingQuirks : 1;
        bool m_forceCompositingMode : 1;
        bool m_shouldInjectUserScriptsInInitialEmptyDocument : 1;
#ifdef ANDROID_META_SUPPORT
        // default is yes
        bool m_viewport_user_scalable : 1;
        // range is from 70 to 400. 0 is a special value means device-dpi
        // default is -1, which means undefined.
        int m_viewport_target_densitydpi;
        // default is yes
        bool m_format_detection_telephone : 1;
        // default is yes
        bool m_format_detection_address : 1;
        // default is yes
        bool m_format_detection_email : 1;
        bool m_default_format_detection : 1;
#endif
#ifdef ANDROID_LAYOUT
        bool m_useWideViewport : 1;
#endif
#ifdef ANDROID_MULTIPLE_WINDOWS
        bool m_supportMultipleWindows : 1;
#endif
#ifdef ANDROID_BLOCK_NETWORK_IMAGE
        bool m_blockNetworkImage : 1;
#endif
#if ENABLE(WEB_AUTOFILL)
        bool m_autoFillEnabled: 1;
#endif
#ifdef ANDROID_PLUGINS
        bool m_pluginsOnDemand : 1;
#endif
	//SISO_HTMLCOMPOSER begin  
	bool m_editableSupportEnabled : 1;
	//SISO_HTMLCOMPOSER end 

		//SAMSUNG_CHANGES >>
		bool m_rssSniffingnEnabled : 1;
#if ENABLE(SPELLCHECK)
		bool m_isspellcheckenabled : 1;// SAMSUNG CHANGES
#endif
		//SAMSUNG_CHANGES <<
#if USE(SAFARI_THEME)
        static bool gShouldPaintNativeControls;
#endif
#if PLATFORM(WIN) || (OS(WINDOWS) && PLATFORM(WX))
        static bool gShouldUseHighResolutionTimers;
#endif

#ifdef WEBKIT_TEXT_SIZE_ADJUST
	//SAMSUNG CHANGE :TEXT READABILITY <<
        bool m_TextReadability :1;
 	    float m_TextReadabilityRatio;
        //SAMSUNG CHANGE :TEXT READABILITY >>
#endif

        //SAMSUNG CHANGE ADVANCED TEXT SELECTION
        bool m_advancedSelectionEnabled : 1;
        Color m_advancedSelectionBgColor;

	bool m_woffEnabled : 1; //SISO WOFF CHANGES 
    };

} // namespace WebCore

#endif // Settings_h
