#include <windows.h>
#include "tp_stub.h"
#include "ncbind.hpp"
#include <string>
#include "SteamAchievements.h"

#include <steam_api.h>
#pragma comment(lib,"steam_api.lib")

//ライブラリ互換がロストしてるようなので仮対策・機能的に無効かどうかは不明

#ifndef BroadcastUploadStart_t
STEAM_CALLBACK_BEGIN(BroadcastUploadStart_t, k_iClientVideoCallbacks + 4)
STEAM_CALLBACK_END(0)
#endif

#ifndef BroadcastUploadStop_t
STEAM_CALLBACK_BEGIN(BroadcastUploadStop_t, k_iClientVideoCallbacks + 5)
STEAM_CALLBACK_MEMBER(0, EBroadcastUploadResult, m_eResult)
STEAM_CALLBACK_END(1)
#endif


// ttstrをUTF8文字列へ変換
std::string
convertTtstrToUtf8String(ttstr &buf)
{
  int maxlen = buf.length() * 6 + 1;
  char *dat = new char[maxlen];
  int len = TVPWideCharToUtf8String(buf.c_str(), dat);
  std::string result(dat, len);
  delete[] dat;
  return result;
}

// std::stringをttstrに変換
ttstr
convertUtf8StringToTtstr(const char *buf, size_t length=-1)
{
	if (length < 0) {
		length = strlen(buf);
	}
	tjs_uint maxlen = (tjs_uint)length * 2 + 1;
	tjs_char *dat = new tjs_char[maxlen];
	tjs_uint len = TVPUtf8ToWideCharString(buf, dat);
	ttstr result(dat, len);
	delete[] dat;
	return result;
}

ttstr
convertUtf8StringToTtstr(const std::string &buf)
{
	return convertUtf8StringToTtstr(buf.c_str(), buf.length());
}

extern void initStorage();
extern void doneStorage();

class SteamScreenshotCallback;
class SteamBroadcastCallback;
class LayerImageToRGB;

/**
 * Steam情報基礎クラス
 */
class Steam : public tTVPContinuousEventCallbackIntf {
public:
	// 初期化
	static void registerSteam() {
		if (!instance) { // added
			instance = new Steam();
			instance->init();
			initStorage();
		} //added
	}
	// 解除
	static void unregisterSteam() {
		if (instance) { //added
			doneStorage();
			delete instance;
		} //added
	}

	//vvv[added]
	// 遅延初期化対応
	class SteamDelayRegister : public tTJSDispatch {
		tTJSVariantClosure closure;
		bool terminate;
		bool PassThroughCheck(const tjs_char * membername, tjs_uint32 *hint) {
			TVPAddLog(TVPFormatMessage(TJS_W("PassThroughCheck %1"), ttstr(membername)));
			if (membername && ttstr(membername) == TJS_W("RestartAppIfNecessary")) return true; // ※メソッド名を合わせること
			if (membername && !terminate) {
				tTJSVariant v;
				if (TJS_SUCCEEDED(closure.PropGet(0, membername, hint, &v, 0))) {
					Steam::registerSteam(); // 遅延初期化実行
					SetClosure(TJS_W("Steam"), closure);
					this->Release();
					terminate = true;
				}
			}
			return false;
		}
		static bool GetClosure(const tjs_char * membername, tTJSVariantClosure &closure) {
			bool r = false;
			iTJSDispatch2 * global = TVPGetScriptDispatch();
			if (global) {
				tTJSVariant v;
				if (TJS_SUCCEEDED(global->PropGet(0, membername, 0, &v, global))) {
					closure = v.AsObjectClosure();
					r = true;
				}
				global->Release();
			}
			return r;
		}
		static bool SetClosure(const tjs_char * membername, const tTJSVariantClosure &closure) {
			bool r = false;
			iTJSDispatch2 * global = TVPGetScriptDispatch();
			if (global) {
				tTJSVariant v(closure.Object, closure.ObjThis);
				r = TJS_SUCCEEDED(global->PropSet(TJS_MEMBERENSURE|TJS_IGNOREPROP, membername, 0, &v, global));
				global->Release();
			}
			return r;
		}
	public:
		SteamDelayRegister() : closure(NULL,NULL), terminate(false) {
			GetClosure(TJS_W("Steam"), closure);
		}
		~SteamDelayRegister() {
			closure.Release();
		}
		void SetSelf() {
			SetClosure(TJS_W("Steam"), tTJSVariantClosure(this, this));
		}
		// とりあえず FuncCall と PropGet だけ実装すれば十分と思われる
		tjs_error TJS_INTF_METHOD FuncCall(tjs_uint32 flag, const tjs_char * membername, tjs_uint32 *hint, tTJSVariant *result, tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *objthis) {
			PassThroughCheck(membername, hint);
			return closure.FuncCall(flag, membername, hint, result, numparams, param, objthis);
		}
		tjs_error TJS_INTF_METHOD PropGet(tjs_uint32 flag, const tjs_char * membername, tjs_uint32 *hint, tTJSVariant *result, iTJSDispatch2 *objthis) {
			PassThroughCheck(membername, hint);
			return closure.PropGet(flag, membername, hint, result, objthis);
		}
	};
	static void setupSteam() {
		SteamDelayRegister *delay = new SteamDelayRegister(); // Steam.*参照で自動破棄される
		delay->SetSelf();
	}
	//^^^[added]

	// Steamクライアント起動チェック
	static bool RestartAppIfNecessary(tTVInteger appid) {
		return SteamAPI_RestartAppIfNecessary(static_cast<uint32>(appid));
	}

	// コンストラクタ用(常に例外)
	static tjs_error Factory(Steam **obj, tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *objthis) {
		TVPThrowExceptionMessage(TJSGetMessageMapMessage(TJS_W("TVPCannotCreateInstance")).c_str());
		return TJS_E_FAIL;
	}

	// コンストラクタ
	Steam() : inited(false), achieve(0), screenshot(0), broadcast(0) {}

	// デストラクタ
	virtual ~Steam();

	// 初期化
	void init() {
		if ((inited = SteamAPI_Init())) {
			ISteamUtils *utils = SteamUtils();
			if (utils) {
				utils->SetOverlayNotificationPosition(k_EPositionTopLeft);
			}
			achieve = new SteamAchievements();
			TVPAddContinuousEventHook(this);
		} else {
			TVPThrowExceptionMessage(L"Steam must be running to play this game (SteamAPI_Init() failed)");
		}
	}

	// ---------------------------------------------------------
	// 実績
	// ---------------------------------------------------------

	static bool requestInitialize() {
		if (instance && instance->achieve) {
			return instance->achieve->requestInitialize();
		}
		return false;
	}

	static bool getInitialized() {
		if (instance && instance->achieve) {
			return instance->achieve->getInitialized();
		}
		return false;
	}
	
	static int getAchievementsCount() {
		if (instance && instance->achieve) {
			return instance->achieve->getAchievementsCount();
		}
		return 0;
	}
	
	static tTJSVariant getAchievement(tTJSVariant n) {
		if (instance && instance->achieve) {
			return instance->achieve->getAchievement(n);
		}
		return tTJSVariant();
	}

	static bool setAchievement(tTJSVariant n) {
		if (instance && instance->achieve) {
			return instance->achieve->setAchievement(n);
		}
		return false;
	}

	static bool clearAchievement(tTJSVariant n) {
		if (instance && instance->achieve) {
			return instance->achieve->clearAchievement(n);
		}
		return false;
	}

	// ---------------------------------------------------------
	// 情報
	// ---------------------------------------------------------
	
	// 現在の言語を取得
	static ttstr getLanguage() {
		ttstr ret;
		ISteamApps *app = SteamApps();
		if (app) {
			std::string lang = app->GetCurrentGameLanguage();
			ret = convertUtf8StringToTtstr(lang);
		}
		return ret;
	}

	// ---------------------------------------------------------
	// クラウド
	// ---------------------------------------------------------

	static bool getCloudEnabled() {
		ISteamRemoteStorage *storage = SteamRemoteStorage();
		if (storage) {
			return storage->IsCloudEnabledForApp();
		}
		return false;
	}

	static void setCloudEnabled(bool enabled) {
		ISteamRemoteStorage *storage = SteamRemoteStorage();
		if (storage) {
			storage->SetCloudEnabledForApp(enabled);
		}
	}

	static tTJSVariant getCloudQuota() {
		tTJSVariant ret;
		ISteamRemoteStorage *storage = SteamRemoteStorage();
		if (storage) {
			uint64 total;
			uint64 available;
			if (storage->GetQuota(&total, &available)) {
				iTJSDispatch2 *dict = TJSCreateDictionaryObject();
				if (dict) {
					ncbPropAccessor obj(dict);
					obj.SetValue(L"total", static_cast<tTVInteger>(total));
					obj.SetValue(L"available", static_cast<tTVInteger>(available));
					ret = tTJSVariant(dict, dict);
					dict->Release();
				}
			}
		}
		return ret;
	}

	static int getCloudFileCount() {
		ISteamRemoteStorage *storage = SteamRemoteStorage();
		if (storage) {
			return storage->GetFileCount();
		}
		return 0;
	}

	static tTJSVariant getCloudFileInfo(int n) {
		tTJSVariant ret;
		ISteamRemoteStorage *storage = SteamRemoteStorage();
		if (storage) {
			int32 size;
			std::string name = storage->GetFileNameAndSize(n, &size);
			int64 time = storage->GetFileTimestamp(name.c_str());
			iTJSDispatch2 *dict = TJSCreateDictionaryObject();
			if (dict) {
				ncbPropAccessor obj(dict);
				obj.SetValue(L"filename", convertUtf8StringToTtstr(name));
				obj.SetValue(L"size", size);
				obj.SetValue(L"time", time);
				ret = tTJSVariant(dict, dict);
				dict->Release();
			}
		}
		return ret;
	}

	static bool deleteCloudFile(ttstr name) {
		bool ret = false;
		ISteamRemoteStorage *storage = SteamRemoteStorage();
		if (storage) {
			ttstr n = name;
			n.ToLowerCase();
			std::string filename = convertTtstrToUtf8String(n);
			ret = storage->FileDelete(filename.c_str());
		}
		return ret;
	}

	static bool copyCloudFile(ttstr src, ttstr dest) {
		bool ret = false;
		ISteamRemoteStorage *storage = SteamRemoteStorage();
		if (storage) {
			ttstr n = src;
			n.ToLowerCase();
			std::string srcfile = convertTtstrToUtf8String(n);
			n = dest;
			n.ToLowerCase();
			std::string destfile = convertTtstrToUtf8String(n);
			if (storage->FileExists(srcfile.c_str())) {
				int size = storage->GetFileSize(srcfile.c_str());
				unsigned char *buffer = new unsigned char[size];
				if (buffer) {
					if (storage->FileRead(srcfile.c_str(), buffer, size) == size) {
						if (storage->FileWrite(destfile.c_str(), buffer, size)) {
							ret = true;
						}
					}
					delete[] buffer;
				}
			}
		}
		return ret;
	}
	// ---------------------------------------------------------
	// スクリーンショット制御
	// ---------------------------------------------------------
	
	/**
	 * スクリーンショット処理をアプリ側から起動させる
	 */
	static void triggerScreenshot() {
		ISteamScreenshots *shots = SteamScreenshots();
		if (shots) {
			shots->TriggerScreenshot();
		}
	}

	/**
	 * @param callback をフックするコールバック関数(voidなら開放)
	 * @return 登録・解除に成功したらtrue
	 */
	static bool hookScreenshots(tTJSVariant callback) {
		if (instance) {
			return instance->_hookScreenshots(callback);
		}
		return false;
	}
	
	/**
	 * スクリーンショットの登録
	 * @param layer 画像
	 * @param location 場所文字列
	 */
	static void writeScreenshot(iTJSDispatch2 *layer, ttstr location) {
		if (instance) {
			instance->_writeScreenshot(layer, location);
		}
	}

	// ---------------------------------------------------------
	// ブロードキャスト制御
	// ---------------------------------------------------------
	
	static bool isBroadcasting() {
		int numViewers = 0;
		ISteamVideo *video = SteamVideo();
		return video && video->IsBroadcasting(&numViewers);
	}

	static bool hookBroadcasting(tTJSVariant callback) {
		if (instance) {
			return instance->_hookBroadcasting(callback);
		}
		return false;
	}

	// ---------------------------------------------------------
	// DLC情報
	// ---------------------------------------------------------


	// only use this member if you need to check ownership of another game related to yours, a demo for example
	static bool isIsSubscribedApp(uint32 appID) {
		bool ret = false;
		ISteamApps *app = SteamApps();
		if (app) {
			ret = app->BIsSubscribedApp( (AppId_t)appID );
		}
		return ret;
	}

	// Takes AppID of DLC and checks if the user owns the DLC & if the DLC is installed
	static bool isDlcInstalled(uint32 appID) {
		bool ret = false;
		ISteamApps *app = SteamApps();
		if (app) {
			ret = app->BIsDlcInstalled( (AppId_t)appID );
		}
		return ret;
	}

	/**
	 * @return DLCの個数を返す
	 */
	static int getDLCCount() {
		int ret = 0;
		ISteamApps *app = SteamApps();
		if (app) {
			ret = app->GetDLCCount();
		}
		return ret;
	}

	/**
	 * @param no DLCの番号 
	 * @rerurn DLC情報
	      appID
		  aAvailable
		  chName
	 * 
	 */
	static tTJSVariant getDLCData(int no) {
		tTJSVariant ret;
		ISteamApps *app = SteamApps();
		if (app) {
			AppId_t appId;
			bool available;
			char chName[4096];
			if (app->BGetDLCDataByIndex(no, &appId, &available, chName, sizeof chName-1)) {
				ttstr name = convertUtf8StringToTtstr(chName);
				iTJSDispatch2 *dict = TJSCreateDictionaryObject();
				if (dict) {
					ncbPropAccessor obj(dict);
					obj.SetValue(L"appId", appId);
					obj.SetValue(L"available", available);
					obj.SetValue(L"chName", name);
					ret = tTJSVariant(dict, dict);
					dict->Release();
				}
			}
		}
		return ret;
	}

public:
	virtual void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick) {
		SteamAPI_RunCallbacks();
	}

protected:
	static Steam *instance;

	bool _hookScreenshots(tTJSVariant callback);
	bool _hookBroadcasting(tTJSVariant callback);
	void _writeScreenshot(iTJSDispatch2 *layer, ttstr location);
	
	bool inited;
	SteamAchievements *achieve;
	SteamScreenshotCallback *screenshot;
	SteamBroadcastCallback  *broadcast;

};


/**
 * SteamCallbackクラス
 */
class SteamCallback : public tTJSVariantClosure {
public:
	SteamCallback() : tTJSVariantClosure(NULL, NULL) {}
	virtual ~SteamCallback() { Release(); }
	bool setCallback(tTJSVariant const &var) {
		if (var.Type() == tvtObject) {
			tTJSVariantClosure closure(var.AsObjectClosureNoAddRef());
			Object  = closure.Object;
			ObjThis = closure.ObjThis;
			AddRef();
		} else {
			Object  = NULL;
			ObjThis = NULL;
		}
		return Object != NULL;
	}
	bool invokeCallback(tjs_uint numparams , tTJSVariant * * params) const {
		tTJSVariant result;
		return Object && Try_iTJSDispatch2_FuncCall(Object, 0, NULL, NULL, &result, numparams, params, ObjThis) == TJS_S_OK;
	}
private:
	tTJSVariantClosure callback;
};

/**
 * SteamScreenshotCallbackクラス
 */
class SteamScreenshotCallback : public SteamCallback {
public:
	SteamScreenshotCallback() : SteamCallback()
		,m_CallbackScreenshotRequested(this, &SteamScreenshotCallback::OnScreenShotRequested)
//		,m_CallbackScreenshotReady(this, &SteamScreenshotCallback::OnScreenShotReady)
	{
	}
	STEAM_CALLBACK(SteamScreenshotCallback, OnScreenShotRequested, ScreenshotRequested_t, m_CallbackScreenshotRequested);
//	STEAM_CALLBACK(SteamScreenshotCallback, OnScreenShotReady, ScreenshotReady_t, m_CallbackScreenshotReady);
};
void
SteamScreenshotCallback::OnScreenShotRequested(ScreenshotRequested_t *pCallback)
{
	// 恐らくSteamAPI_RunCallbacks() のタイミングで呼ばれると思われる
	invokeCallback(0, NULL);

	// FuncCall でなくて TVPPostEvent なんたらのがよかったかも
	//::TVPPostEvent(source, target, TJS_W("onSteamScreenShotRequested"), 0, TVP_EPT_EXCLUSIVE, numargs, args);
}
/*
// スクリーンショットを撮った後に呼ばれる？（タグ付けやlocationを設定する場合など）
void
SteamScreenshotCallback::OnScreenShotReady(ScreenshotReady_t *pCallback)
{
	ISteamScreenshots *shots = SteamScreenshots();
	if (shots) {
		ScreenshotHandle hScreenshot = pCallback->m_hLocal;

		//shorts->SetLocation(hScreenshot, pchLocation);
		//shorts->TagPublishedFile(hScreenshot, unPublishedFileID);
	}
}
 */


/**
 * SteamBroadcastCallbackクラス
 */
class SteamBroadcastCallback : public SteamCallback {
public:
	SteamBroadcastCallback() : SteamCallback()
		,m_CallbackBroadcastStart(this, &SteamBroadcastCallback::OnBroadcastStart)
		,m_CallbackBroadcastStop(this, &SteamBroadcastCallback::OnBroadcastStop)
	{
	}
	STEAM_CALLBACK(SteamBroadcastCallback, OnBroadcastStart, BroadcastUploadStart_t, m_CallbackBroadcastStart);
	STEAM_CALLBACK(SteamBroadcastCallback, OnBroadcastStop, BroadcastUploadStop_t, m_CallbackBroadcastStop);
};
void
SteamBroadcastCallback::OnBroadcastStart(BroadcastUploadStart_t *pCallback)
{
	tTJSVariant state(1), *args = &state;
	invokeCallback(1, &args);
}
void
SteamBroadcastCallback::OnBroadcastStop(BroadcastUploadStop_t *pCallback)
{
	tTJSVariant state(0), *args = &state;
	invokeCallback(1, &args);
}


class    LayerImageToRGB {
public:
	/**/ LayerImageToRGB() : buffer(0), size(0), width(0), height(0) {}
	/**/~LayerImageToRGB() { term(); }

	void init(iTJSDispatch2 *lay) {
		term();
		if (!lay) return;

		const BYTE *ptr;
		long pitch;
		if (!GetLayerSize(lay, width, height) ||
			!GetLayerImage(lay, ptr, pitch)) return;

		size = width * height * 3;
		buffer = new BYTE[size];

		BYTE *w = buffer;
		for (size_t y = 0; y < height; y++) {
			const BYTE* r = ptr + pitch * y;
			for (size_t x = 0; x < width; x++, r+=4) {
				*w++ = r[2];
				*w++ = r[1];
				*w++ = r[0];
			}
		}
	}

	void term() {
		if (buffer) delete[] buffer;
		buffer = 0;
		size = width = height = 0;
	}

	inline void  *getRGB()   const { return buffer; }
	inline uint32 getSize()  const { return size;   }
	inline int   getWidth()  const { return width;  }
	inline int   getHeight() const { return height; }

private:
	BYTE *buffer;
	size_t size, width, height;

	static iTJSDispatch2 *LayerClass;
	static bool GetLayerSize(iTJSDispatch2 *lay, size_t &w, size_t &h) {
		static ttstr hasImage   (TJS_W("hasImage"));
		static ttstr imageWidth (TJS_W("imageWidth"));
		static ttstr imageHeight(TJS_W("imageHeight"));

		tTVInteger lw, lh;
		if (!LayerPropGet(lay, hasImage) ||
			(lw = LayerPropGet(lay, imageWidth )) <= 0 ||
			(lh = LayerPropGet(lay, imageHeight)) <= 0) return false;
		w = (size_t)lw;
		h = (size_t)lh;
		return true;
	}
	static bool GetLayerImage(iTJSDispatch2 *lay, const BYTE* &ptr, long &pitch) {
		static ttstr mainImageBufferPitch(TJS_W("mainImageBufferPitch"));
		static ttstr mainImageBuffer(TJS_W("mainImageBuffer"));

		tTVInteger lpitch, lptr;
		if ((lpitch = LayerPropGet(lay, mainImageBufferPitch)) == 0 ||
			(lptr   = LayerPropGet(lay, mainImageBuffer)) == 0) return false;
		pitch = (long)lpitch;
		ptr = reinterpret_cast<const BYTE*>(lptr);
		return true;
	}
	static tTVInteger LayerPropGet(iTJSDispatch2 *lay, ttstr &prop, tTVInteger defval = 0) {
		if (!LayerClass) {
			tTJSVariant var;
			TVPExecuteExpression(TJS_W("Layer"), &var);
			LayerClass = var.AsObjectNoAddRef();
		}
		tTJSVariant val;
		return (TJS_FAILED(LayerClass->PropGet(0, prop.c_str(), prop.GetHint(), &val, lay))) ? defval : val.AsInteger();
	}
};
iTJSDispatch2 *LayerImageToRGB::LayerClass = 0;

// ---------------------------------------------------------
// 実装

Steam::~Steam() {
	if (achieve) {
		delete achieve;
		achieve = 0;
	}
	if (screenshot) {
		delete screenshot;
		screenshot = 0;
	}
	if (broadcast) {
		delete broadcast;
		broadcast = 0;
	}
	if (inited){
		TVPRemoveContinuousEventHook(this);
		SteamAPI_Shutdown();
	}
}
bool Steam::_hookScreenshots(tTJSVariant callback) {
	ISteamScreenshots *shots = SteamScreenshots();
	if (shots) {
		if(!screenshot) {
			screenshot = new SteamScreenshotCallback();
		}
		if (screenshot) {
			bool bHook = screenshot->setCallback(callback);
			shots->HookScreenshots(bHook);
			return true;
		}
	}
	return false;
}

bool Steam::_hookBroadcasting(tTJSVariant callback) {
	if(!broadcast) {
		broadcast = new SteamBroadcastCallback();
	}
	if (broadcast) {
		broadcast->setCallback(callback);
		return true;
	}
	return false;
}

void Steam::_writeScreenshot(iTJSDispatch2 *layer, ttstr location) {
	if (!layer) return;
	ISteamScreenshots *shots = SteamScreenshots();
	if (shots) {
		LayerImageToRGB *image = new LayerImageToRGB();
		try {
			image->init(layer);
			ScreenshotHandle handle = shots->WriteScreenshot(image->getRGB(), image->getSize(), image->getWidth(), image->getHeight());
			if (!location.IsEmpty()) {
				std::string text = convertTtstrToUtf8String(location);
				shots->SetLocation(handle, text.c_str());
			}
		} catch (...) {
			delete image;
			throw;
		}
		delete image;
	}
}




Steam *Steam::instance = 0;

NCB_REGISTER_CLASS(Steam) {
	Factory(&Class::Factory);
	NCB_METHOD(getLanguage);

	NCB_METHOD(RestartAppIfNecessary); // added

	NCB_METHOD(requestInitialize);
	NCB_PROPERTY_RO(initialized, getInitialized);
	NCB_PROPERTY_RO(achievementsCount, getAchievementsCount);
	NCB_METHOD(getAchievement);
	NCB_METHOD(setAchievement);
	NCB_METHOD(clearAchievement);

	NCB_PROPERTY(cloudEnabled, getCloudEnabled, setCloudEnabled);
	NCB_METHOD(getCloudQuota);
	NCB_PROPERTY_RO(cloudFileCount, getCloudFileCount);
	NCB_METHOD(getCloudFileInfo);
	NCB_METHOD(deleteCloudFile);
	NCB_METHOD(copyCloudFile);

	NCB_METHOD(triggerScreenshot);
	NCB_METHOD(hookScreenshots);
	NCB_METHOD(writeScreenshot);

	NCB_METHOD(isBroadcasting);
	NCB_METHOD(hookBroadcasting);

	NCB_METHOD(isIsSubscribedApp);
	NCB_METHOD(isDlcInstalled);
	NCB_METHOD(getDLCCount);
	NCB_METHOD(getDLCData);
}



//NCB_REGISTER_CALLBACK(PreRegist,  Steam::registerSteam, 0, registerSteam_0); // CUT
NCB_REGISTER_CALLBACK(PostRegist, Steam::setupSteam, 0, 0_setupSteam); //added
NCB_REGISTER_CALLBACK(PostRegist, 0, Steam::unregisterSteam, 0_unregisterSteam);