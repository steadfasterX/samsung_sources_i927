#include <config.h>
#include <stdio.h>
#if ENABLE(WRT)
#ifndef _LIBS_UTILS_LOG_H
            #undef LOG
            #include <utils/Log.h>
 #endif
#include "WrtNativeprovider.h"
 
 #define LOG_TAG "WRT"
// using namespace android;
namespace android {

WrtNativeprovider* WrtNativeprovider::pWrtNativeProvider = NULL;

WrtNativeprovider* WrtNativeprovider::Instance()
{
	LOGE("WrtNativeprovider::Instance");
	if( !pWrtNativeProvider ) 
	{
		LOGE("WrtNativeprovider::Instance created");
		pWrtNativeProvider = new WrtNativeprovider();
	}
	return pWrtNativeProvider;
}
WrtNativeprovider::~WrtNativeprovider()
	{
		//Iterate through the list 
		// Remove from the list
		// Release the ref
	LOGE("WrtNativeprovider::~WrtNativeprovider");
	unsigned int count = widgets.size();

	for( unsigned int index = 0;index < count; index++ )
		{
		WidgetInfo* widget= widgets.itemAt(index);
		Release(widget->mWebCore);			
		widgets.removeAt(index);

		}
	}

void WrtNativeprovider::Add( WebViewCore* webCore )
	{
	LOGE("WrtNativeprovider::Add WebViewCore 0x%x",webCore);
	WidgetInfo* widgetInfo = new WidgetInfo();
	Retain(webCore); // Rupesh
	widgetInfo->mWebCore =  webCore;
	widgets.add(widgetInfo);
	}


void WrtNativeprovider::Remove( WebViewCore* webCore )
{
	LOGE("WrtNativeprovider::Remove WebViewCore");
	unsigned int count = widgets.size();
	for( unsigned int index = 0;index < count; index++ )
	{
		WidgetInfo* widget= widgets.itemAt(index);
		if(widget->mWebCore == webCore )
		{			
			//Remove from the list
			Release(widget->mWebCore);
			widgets.removeAt(index);
			break;
		}
	}
}

void WrtNativeprovider::AddWidgetView ( WidgetView* widgetView, jobject jwebView )
	{
	unsigned int count = widgets.size();
	for( unsigned int index = 0;index < count; index++ )
		{
		WidgetInfo* widget= widgets.itemAt(index);
		if( widget->mWebCore->getWebViewJavaObject() == jwebView )
			{
			LOGE("WrtNativeprovider::widgetView added at %d Webcore-0x%x WidgetView-0x%x JavaWebView-0x%x",index,widget->mWebCore,widgetView,jwebView);
			widget->mWidgetView = widgetView;
			widgets.replaceAt(widget,index);
			break;
			}
		}
	}
WidgetView* WrtNativeprovider::GetWidgetView(WebViewCore* webCore )
	{
	unsigned int count = widgets.size();
	WidgetInfo* widget= NULL;
	for( unsigned int index = 0;index < count; index++ )
		{
		WidgetInfo* widget= widgets.itemAt(index);
		if( widget->mWebCore == webCore )
			{
			break;
			}
		}
	if(widget)
		return widget->mWidgetView;
	else	
	    return NULL;
	}
WidgetView* WrtNativeprovider::GetWidgetView(jobject jwebView )
	{
	unsigned int count = widgets.size();
	WidgetInfo* widget=NULL;
	for( unsigned int index = 0;index < count; index++ )
		{
		widget= widgets.itemAt(index);
		if( widget->mWebCore->getWebViewJavaObject() == jwebView )
			{
			LOGE("WrtNativeprovider::GetWidgetView %d Java WebView0x%x",index,jwebView);
			break;
			}
		}
	if(widget)
		return widget->mWidgetView;
	else	
	    return NULL;
	}
void WrtNativeprovider::SetWrtIds( jobject jwebView,int runtimeId, int widgetId )
	{
	unsigned int count = widgets.size();
	for( unsigned int index = 0;index < count; index++ )
		{
		WidgetInfo* widget= widgets.itemAt(index);
		widget->mRtId = runtimeId;
		widget->widgetId = widgetId;
		if( widget->mWebCore->getWebViewJavaObject() == jwebView )
			{
			widget->mWebCore->setIDForWidgets(runtimeId,widgetId);
			break;
			}
		}

	}
void WrtNativeprovider::RemoveWidgetView( jobject jwebView )
	{
	LOGE("WrtNativeprovider::RemoveWidgetView JavaWebview 0x%x",jwebView);
	unsigned int count = widgets.size();
	for( unsigned int index = 0;index < count; index++ )
		{
		WidgetInfo* widget= widgets.itemAt(index);
		LOGE("WrtNativeprovider::RemoveWidgetView %d",index);
		if( widget->mWebCore->getWebViewJavaObject() == jwebView )
			{
			//Remove from the list
			LOGE("WrtNativeprovider::RemoveWidgetView Success ");
			Release(widget->mWebCore);
			widgets.removeAt(index);			
			break;
			}
		}
	}

WidgetView* WrtNativeprovider::GetWidgetView(int rtId )
	{
	unsigned int count = widgets.size();
	WidgetInfo* widget=NULL;
	for( unsigned int index = 0;index < count; index++ )
		{
		widget= widgets.itemAt(index);
		if( widget->mRtId == rtId )
			{
			break;
			}
		}
	if(widget)
		return widget->mWidgetView;
	else	
	    return NULL;
	}
}
#endif