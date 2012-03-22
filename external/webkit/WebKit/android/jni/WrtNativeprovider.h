#ifndef _WRTNATIVEPROVIDER_H
#define _WRTNATIVEPROVIDER_H

//#if ENABLE(WRT)
#include "WidgetView.h"
#include "WebViewCore.h"
#include <utils/Vector.h>

using namespace WebCore;

namespace android {
class WidgetInfo{
		
	public:
	inline void SetWidgetView(WidgetView* pWidgetView )
		{
		mWidgetView = pWidgetView;
		}
	inline void SetWebViewCore( WebViewCore* pWebViewCore )
		{
		mWebCore = pWebViewCore;
		}
	inline WidgetView* GetWidgetView()
		{
		return mWidgetView;
		}
	inline WebViewCore* GetWebViewCore( )
		{
		return mWebCore;
		}
	public:
		
	WidgetView* mWidgetView;
	WebViewCore* mWebCore;
	int mRtId;
	int widgetId;
};
class WrtNativeprovider{

protected:
	WrtNativeprovider(){};
public:
	~WrtNativeprovider();
	static WrtNativeprovider* Instance();
	
	void Add( WebViewCore* webCore );
	void Remove( WebViewCore* webCore );
	void AddWidgetView( WidgetView* widgetView, jobject jwebView );
	WidgetView* GetWidgetView(WebViewCore* webCore );
	WidgetView* GetWidgetView(jobject jwebView );
	WidgetView* GetWidgetView(int rtId );
	void SetWrtIds( jobject jwebView,int runtimeId, int widgetId );
	void RemoveWidgetView( jobject jwebView );
private:
static WrtNativeprovider* pWrtNativeProvider;
Vector<WidgetInfo*> widgets;
};
};

//#endif
#endif

/*******************End of header file***************/
