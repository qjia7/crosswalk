// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_RUNTIME_BROWSER_ANDROID_XWALK_CONTENTS_CLIENT_BRIDGE_H_
#define XWALK_RUNTIME_BROWSER_ANDROID_XWALK_CONTENTS_CLIENT_BRIDGE_H_

#include <jni.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/id_map.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "xwalk/runtime/browser/android/xwalk_contents_client_bridge_base.h"

namespace gfx {
class Size;
}

namespace net {
class X509Certificate;
}

class SkBitmap;

namespace xwalk {

// A class that handles the Java<->Native communication for the
// XWalkContentsClient. XWalkContentsClientBridge is created and owned by
// native XWalkViewContents class and it only has a weak reference to the
// its Java peer. Since the Java XWalkContentsClientBridge can have
// indirect refs from the Application (via callbacks) and so can outlive
// XWalkView, this class notifies it before being destroyed and to nullify
// any references.
class XWalkContentsClientBridge : public XWalkContentsClientBridgeBase {
 public:
  XWalkContentsClientBridge(JNIEnv* env, jobject obj);
  virtual ~XWalkContentsClientBridge();

  // XWalkContentsClientBridgeBase implementation
  virtual void AllowCertificateError(int cert_error,
                                     net::X509Certificate* cert,
                                     const GURL& request_url,
                                     const base::Callback<void(bool)>& callback, // NOLINT
                                     bool* cancel_request) OVERRIDE;

  virtual void RunJavaScriptDialog(
      content::JavaScriptMessageType message_type,
      const GURL& origin_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      OVERRIDE;
  virtual void RunBeforeUnloadDialog(
      const GURL& origin_url,
      const base::string16& message_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      OVERRIDE;
  virtual void ShowNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      content::RenderFrameHost* render_frame_host,
      content::DesktopNotificationDelegate* delegate,
      base::Closure* cancel_callback)
      OVERRIDE;
  virtual void UpdateNotificationIcon(
      int notification_id,
      const SkBitmap& icon)
      OVERRIDE;
  virtual void OnWebLayoutPageScaleFactorChanged(
      float page_scale_factor)
      OVERRIDE;

  bool OnReceivedHttpAuthRequest(const base::android::JavaRef<jobject>& handler,
                                 const std::string& host,
                                 const std::string& realm);

  void OnNotificationIconDownloaded(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  // Methods called from Java.
  void ProceedSslError(JNIEnv* env, jobject obj, jboolean proceed, jint id);
  void ConfirmJsResult(JNIEnv*, jobject, int id, jstring prompt);
  void CancelJsResult(JNIEnv*, jobject, int id);
  void ExitFullscreen(JNIEnv*, jobject, jlong web_contents);
  void NotificationDisplayed(JNIEnv*, jobject, jlong delegate);
  void NotificationError(JNIEnv*, jobject, jlong delegate);
  void NotificationClicked(JNIEnv*, jobject, jint id, jlong delegate);
  void NotificationClosed(JNIEnv*, jobject, jint id, bool by_user,
    jlong delegate);
  void OnFilesSelected(
      JNIEnv*, jobject, int process_id, int render_id,
      int mode, jstring filepath, jstring display_name);
  void OnFilesNotSelected(
      JNIEnv*, jobject, int process_id, int render_id, int mode);

 private:
  JavaObjectWeakGlobalRef java_ref_;

  typedef const base::Callback<void(bool)> CertErrorCallback; // NOLINT
  IDMap<CertErrorCallback, IDMapOwnPointer> pending_cert_error_callbacks_;
  IDMap<content::JavaScriptDialogManager::DialogClosedCallback, IDMapOwnPointer>
      pending_js_dialog_callbacks_;

  typedef std::pair<int, content::RenderFrameHost*>
    NotificationDownloadRequestInfos;
  typedef std::map<int, NotificationDownloadRequestInfos >
    NotificationDownloadRequestIdMap;
  NotificationDownloadRequestIdMap downloading_icon_notifications_;
};

bool RegisterXWalkContentsClientBridge(JNIEnv* env);

}  // namespace xwalk

#endif  // XWALK_RUNTIME_BROWSER_ANDROID_XWALK_CONTENTS_CLIENT_BRIDGE_H_
