// Copyright (c) 2014 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/xwalk_render_message_filter.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "xwalk/runtime/common/xwalk_common_messages.h"
#include "xwalk/runtime/browser/runtime_platform_util.h"

namespace {

#if defined(ENABLE_PEPPER_CDMS)

enum PluginAvailabilityStatusForUMA {
  PLUGIN_NOT_REGISTERED,
  PLUGIN_AVAILABLE,
  PLUGIN_DISABLED,
  PLUGIN_AVAILABILITY_STATUS_MAX
};

static void SendPluginAvailabilityUMA(const std::string& mime_type,
                                      PluginAvailabilityStatusForUMA status) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  // Only report results for Widevine CDM.
  if (mime_type != kWidevineCdmPluginMimeType)
    return;

  UMA_HISTOGRAM_ENUMERATION("Plugin.AvailabilityStatus.WidevineCdm",
                            status, PLUGIN_AVAILABILITY_STATUS_MAX);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
}

#endif  // defined(ENABLE_PEPPER_CDMS)

}  // namespace

namespace xwalk {

using content::PluginService;
using content::WebPluginInfo;

XWalkRenderMessageFilter::XWalkRenderMessageFilter()
    : BrowserMessageFilter(ViewMsgStart) {
}

bool XWalkRenderMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(XWalkRenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewMsg_OpenLinkExternal, OnOpenLinkExternal)
#if defined(ENABLE_PEPPER_CDMS)
    IPC_MESSAGE_HANDLER(
        XwalkViewHostMsg_IsInternalPluginAvailableForMimeType,
        OnIsInternalPluginAvailableForMimeType)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void XWalkRenderMessageFilter::OnOpenLinkExternal(const GURL& url) {
  LOG(INFO) << "OpenLinkExternal: " << url.spec();
  platform_util::OpenExternal(url);
}

#if defined(ENABLE_PEPPER_CDMS)

void XWalkRenderMessageFilter::OnIsInternalPluginAvailableForMimeType(
    const std::string& mime_type,
    bool* is_available,
    std::vector<base::string16>* additional_param_names,
    std::vector<base::string16>* additional_param_values) {
  std::vector<WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);

  bool is_plugin_disabled = false;
  for (size_t i = 0; i < plugins.size(); ++i) {
    const WebPluginInfo& plugin = plugins[i];
    const std::vector<content::WebPluginMimeType>& mime_types =
        plugin.mime_types;
    for (size_t j = 0; j < mime_types.size(); ++j) {
      if (mime_types[j].mime_type == mime_type) {
        if (!context_.IsPluginEnabled(plugin)) {
          is_plugin_disabled = true;
          break;
        }

        *is_available = true;
        *additional_param_names = mime_types[j].additional_param_names;
        *additional_param_values = mime_types[j].additional_param_values;
        SendPluginAvailabilityUMA(mime_type, PLUGIN_AVAILABLE);
        return;
      }
    }
  }

  *is_available = false;
  SendPluginAvailabilityUMA(
      mime_type, is_plugin_disabled ? PLUGIN_DISABLED : PLUGIN_NOT_REGISTERED);
}

#endif // defined(ENABLE_PEPPER_CDMS)

}  // namespace xwalk
