#ifndef WIDGETVIEW_H
#define WIDGETVIEW_H

#include <jni.h>
//#include "PlatformString.h"
//#include "wrt_jil_widget.h" //Commented for JSC build
//#include "wrt_widget_common.h" //Commented for JSC build
//#include "wrt_jil_proxywidget.h" //Commented for JSC build

namespace android {
class WidgetView 
{
	public:
		WidgetView (JNIEnv *env, jobject javaWidgetView, jobject javaWebView);
		~WidgetView();
		void Associate(JNIEnv *env, jobject obj,jobject javaWebView,jint runtimeId,jint widgetId );
		int GetRuntimeId();
		int GetWidgetId();

		int nativeOpenUrl(int rtDesc,int wgtDesc,char *url);
		
		
		//WrtJilNative::JilProxyWidgetClass *proxyWidget; //Commented for JSC build

	private:
		struct JavaGlue*       m_javaGlue;
		int mRuntimeId;
		int mWidgetId;
		WidgetView* mWidgetView;
		
	};
}//end of namespace
//#endif
#endif//WIDGETVIEW_H

