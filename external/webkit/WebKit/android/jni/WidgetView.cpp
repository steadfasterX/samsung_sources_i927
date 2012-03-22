#if ENABLE(WRT)
#include <stdio.h>
#include <config.h>
#include "Frame.h"

#ifndef _LIBS_UTILS_LOG_H
            #undef LOG
            #include <utils/Log.h>
 #endif
#include "WidgetView.h"
#include "WebCoreJni.h"
#include <jni.h>
#include <JNIHelp.h>
#include "WrtNativeprovider.h"
#include "PlatformString.h"
#include "CString.h"
#include "string.h"

#include "v8.h"

#if USE(V8)
#include <JNIHelp.h>
#include <JNIUtility.h>
#include "CString.h"
#include "ScriptController.h"
#include "V8Counters.h"
#endif


#define LOG_TAG "WRT"
 
#define GET_WIDGET_VIEW(env, obj) ((WidgetView*)env->GetIntField(obj, gWidgetViewFields.m_nativeClass))

//using namespace WrtJilNative; //Commented for JSC build

namespace android {

struct WidgetViewFields {
    jfieldID    m_nativeClass;
    } gWidgetViewFields;

struct JavaGlue {
    jobject     m_obj;
    jobject     m_javaWebView;
    };

static jmethodID GetJMethod(JNIEnv* env, jclass clazz, const char name[], const char signature[])
{
    jmethodID m = env->GetMethodID(clazz, name, signature);
    LOG_ASSERT(m, "Could not find method %s", name);
    return m;
}

WidgetView::WidgetView(JNIEnv *env, jobject javaWidgetView, jobject javaWebView )
{
    jclass clazz = env->GetObjectClass(javaWidgetView);
    m_javaGlue = new JavaGlue;
    m_javaGlue->m_obj = adoptGlobalRef(env, javaWidgetView);
    m_javaGlue->m_javaWebView = adoptGlobalRef(env, javaWebView);
    //m_javaGlue->m_RuntimeId = GetJMethod(env, clazz, "nativegetRuntimeId", "()Ljava/lang/String;");
    env->SetIntField(javaWidgetView, gWidgetViewFields.m_nativeClass, (jint)this);
    LOGD("WidgetView::WidgetView");
}
void WidgetView::Associate(JNIEnv *env, jobject javaWidgetView,jobject javaWebView,jint runtimeId,jint widgetId)
{
    LOGD("WidgetView::Associate");
//    mRuntimeId = to_string( env, runtimeId );
	mRuntimeId = runtimeId;
	mWidgetId = widgetId;
	WidgetView* view = NULL;
	
	WrtNativeprovider* pWrtNativeprovider = NULL;
	pWrtNativeprovider = WrtNativeprovider::Instance();
	if(pWrtNativeprovider)
	{
		LOGD("WrtNativeprovider valid");
		pWrtNativeprovider->SetWrtIds(javaWebView, runtimeId, widgetId);
		pWrtNativeprovider->AddWidgetView(this, javaWebView);	
		view = pWrtNativeprovider->GetWidgetView(javaWebView);
	}
	else
	{
		LOGE("WrtNativeprovider is NULL");	
	}
		
	int  rtId = -1;
	int  wgId = -1;
	
	if( view )
	{
    	rtId = view->GetRuntimeId();
		wgId = view->GetWidgetId();
	}
	else
	{
		//Suchand added
		view = new WidgetView(env,javaWidgetView,javaWebView);
		rtId = view->GetRuntimeId();
		wgId = view->GetWidgetId();
		LOGE("WidgetView is null so create it");
	}
	
    LOGD("WidgetView::GetRuntimeId : %d",rtId);
	LOGD("WidgetView::GetWidgetId : %d",wgId);
}
int  WidgetView::GetRuntimeId()
{
return mRuntimeId;
}

int  WidgetView::GetWidgetId()
{
return mWidgetId;
}
int WidgetView::nativeOpenUrl(int rtDesc,int wgtDesc,char *url)
{
	return 0;
}

static void nativeOnFocus(JNIEnv *env, jobject obj,jint rtDesc,jint wgtDesc)
{
	void* param=NULL;
	LOGD("WidgetView JNI onFocus runtimeId: %d",rtDesc);
	//WrtJilNative::JilProxyWidgetClass *proxyWidget = new WrtJilNative::JilProxyWidgetClass();

	//InstanceInfo* info = WidgetInstanceProvider::Instance()->getParameters(rtDesc ); //Commented for JSC build
	//WrtJilNative::JilRealWidgetClass *realWidget =  info->instance; //Commented for JSC build
	//realWidget->onFocusCB(info->param);  //Commented for JSC build
	
}
static void nativeOnMaximize(JNIEnv *env, jobject obj,jint rtDesc,jint wgtDesc)
{
	void* param=NULL;
	//WrtJilNative::JilProxyWidgetClass *proxyWidget = new WrtJilNative::JilProxyWidgetClass(); //Commented for JSC build
	//proxyWidget->onMaximize(rtDesc,wgtDesc,param);
	
}
static void nativeOnRestore(JNIEnv *env, jobject obj,jint rtDesc,jint wgtDesc)
{
	void* param=NULL;
	//WrtJilNative::JilProxyWidgetClass *proxyWidget = new WrtJilNative::JilProxyWidgetClass(); //Commented for JSC build
	//proxyWidget->onRestore(rtDesc,wgtDesc,param);
	
}
static void nativeOnWakeup(JNIEnv *env, jobject obj,jint rtDesc,jint wgtDesc)
{
	void* param=NULL;
	//WrtJilNative::JilProxyWidgetClass *proxyWidget = new WrtJilNative::JilProxyWidgetClass(); //Commented for JSC build
	//proxyWidget->onWakeup(rtDesc,wgtDesc,param);
	
}



WidgetView::~WidgetView()
{
    if (m_javaGlue->m_obj) 
   {
   	 LOGD("WidgetView:: ~WidgetView 0x%x",m_javaGlue->m_obj);
        JNIEnv* env = JSC::Bindings::getJNIEnv();
        env->DeleteGlobalRef(m_javaGlue->m_obj);
	  env->DeleteGlobalRef(m_javaGlue->m_javaWebView);
        m_javaGlue->m_obj = 0;
		m_javaGlue->m_javaWebView=0;
    }
    delete m_javaGlue;
    
}

static WidgetView* GetWidgetView( jobject obj )
{
JNIEnv* env = JSC::Bindings::getJNIEnv();
WidgetView* widgetView = GET_WIDGET_VIEW(env, obj);
return widgetView;
}

static void nativeDestroy(JNIEnv *env, jobject obj,jobject javaWebView)
{
	LOGD("WidgetView:: nativeDestroy");		
	//Prashanth-2011/01/07
	WrtNativeprovider* pWrtNativeprovider = NULL;
	pWrtNativeprovider = WrtNativeprovider::Instance();
	if(pWrtNativeprovider)
	{
		if(javaWebView)
		{
		    pWrtNativeprovider->RemoveWidgetView(javaWebView); 
			LOGD("WidgetView::nativeDestroy m_javaWebView 0x%x",javaWebView);
		}
		else
			LOGE("WidgetView::nativeDestroy javaWebView NULL ");
	}
	else
		LOGE("WidgetView::nativeDestroy pWrtNativeprovider NULL ");	
	delete GetWidgetView(obj);	
}

static void nativeCreate(JNIEnv *env, jobject obj,jobject javaWebView,jint runtimeId,jint widgetId)
{
 LOGD("WidgetView:: nativeCreate");
 WidgetView* widgetView = NULL;
 if(javaWebView != NULL)
 {
	widgetView = new WidgetView(env,obj,javaWebView);
	widgetView->Associate(env,obj,javaWebView,runtimeId,widgetId); 
 }
 else
	LOGE("WidgetView:: nativeCreate javaWebView is NULL");	
	
}


// ----------------------------------------------------------------------------

/*
 * JNI registration.
 */
static JNINativeMethod gJavaWidgetViewMethods[] = {
	{"nativeCreate", "(Landroid/webkit/WebView;II)V",(void*)nativeCreate},
	{"nativeDestroy", "(Landroid/webkit/WebView;)V",(void*)nativeDestroy},	
	{"nativeOnFocus", "(II)V",(void*)nativeOnFocus},
	{"nativeOnMaximize", "(II)V",(void*)nativeOnMaximize},
	{"nativeOnRestore", "(II)V",(void*)nativeOnRestore},
	{"nativeOnWakeup", "(II)V",(void*)nativeOnWakeup}
};

int register_widgetview(JNIEnv* env)
{
    LOGE("register_widgetview:: Called");
    jclass widget = env->FindClass("android/samsung/wrt/webwidget/WidgetView");
    LOG_ASSERT(widget,
            "Unable to find class android/samsung/wrt/webwidget/WidgetView");
    gWidgetViewFields.m_nativeClass = env->GetFieldID(widget, "mNativeClass",
            "I");
    LOG_ASSERT(gWidgetViewFields.m_nativeClass,
            "Unable to find android/webkit/WebViewCore.mNativeClass");
     return jniRegisterNativeMethods(env, "android/samsung/wrt/webwidget/WidgetView",
            gJavaWidgetViewMethods, NELEM(gJavaWidgetViewMethods));
}

} /* namespace android */
#endif