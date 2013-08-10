// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/extensions/browser/xwalk_extension_service.h"

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "xwalk/runtime/browser/runtime.h"
#include "xwalk/extensions/browser/xwalk_extension_web_contents_handler.h"
#include "xwalk/extensions/common/xwalk_extension.h"
#include "xwalk/extensions/common/xwalk_extension_messages.h"
#include "xwalk/extensions/common/xwalk_extension_threaded_runner.h"
#include "content/public/browser/render_process_host.h"

namespace xwalk {
namespace extensions {

namespace {

XWalkExtensionService::RegisterExtensionsCallback
g_register_extensions_callback;

}

XWalkExtensionService::XWalkExtensionService(RuntimeRegistry* runtime_registry)
    : runtime_registry_(runtime_registry),
      render_process_host_(NULL) {
  // FIXME(cmarcelo): Once we update Chromium code, replace RuntimeRegistry
  // dependency with callbacks to track WebContents, since we currently don't
  // depend on xwalk::Runtime features.
  runtime_registry_->AddObserver(this);

  if (!g_register_extensions_callback.is_null())
    g_register_extensions_callback.Run(this);
}

XWalkExtensionService::~XWalkExtensionService() {
  runtime_registry_->RemoveObserver(this);
  ExtensionMap::iterator it = extensions_.begin();
  for (; it != extensions_.end(); ++it)
    delete it->second;
}

namespace {

bool ValidateExtensionName(const std::string& extension_name) {
  bool dot_allowed = false;
  bool digit_or_underscore_allowed = false;
  for (size_t i = 0; i < extension_name.size(); ++i) {
    char c = extension_name[i];
    if (IsAsciiDigit(c)) {
      if (!digit_or_underscore_allowed)
        return false;
    } else if (c == '_') {
      if (!digit_or_underscore_allowed)
        return false;
    } else if (c == '.') {
      if (!dot_allowed)
        return false;
      dot_allowed = false;
      digit_or_underscore_allowed = false;
    } else if (IsAsciiAlpha(c)) {
      dot_allowed = true;
      digit_or_underscore_allowed = true;
    } else {
      return false;
    }
  }

  // If after going through the entire name we finish with dot_allowed, it means
  // the previous character is not a dot, so it's a valid name.
  return dot_allowed;
}

}  // namespace

bool XWalkExtensionService::RegisterExtension(XWalkExtension* extension) {
  // Note: for now we only support registering new extensions before
  // render process hosts were created.
  CHECK(!render_process_host_);
  if (extensions_.find(extension->name()) != extensions_.end())
    return false;

  if (!ValidateExtensionName(extension->name())) {
    LOG(WARNING) << "Ignoring extension with invalid name: "
                 << extension->name();
    return false;
  }

  std::string name = extension->name();
  extensions_[name] = extension;
  return true;
}

void XWalkExtensionService::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  // FIXME(cmarcelo): For now we support only one render process host.
  if (render_process_host_)
    return;

  render_process_host_ = host;
  RegisterExtensionsForNewHost(render_process_host_);

  // Attach extensions to already existing runtimes. Related the conditional in
  // OnRuntimeAdded.
  const RuntimeList& runtimes = RuntimeRegistry::Get()->runtimes();
  for (size_t i = 0; i < runtimes.size(); i++)
    CreateWebContentsHandler(runtimes[i]->web_contents());
}

XWalkExtension* XWalkExtensionService::GetExtensionForName(
    const std::string& name) {
  ExtensionMap::iterator it = extensions_.find(name);
  if (it == extensions_.end())
    return NULL;
  return it->second;
}

void XWalkExtensionService::CreateRunnersForHandler(
    XWalkExtensionWebContentsHandler* handler, int64_t frame_id) {
  ExtensionMap::const_iterator it = extensions_.begin();
  for (; it != extensions_.end(); ++it) {
    XWalkExtensionRunner* runner =
        new XWalkExtensionThreadedRunner(it->second, handler);
    handler->AttachExtensionRunner(frame_id, runner);
  }
}

void XWalkExtensionService::OnRuntimeAdded(Runtime* runtime) {
  if (render_process_host_)
    CreateWebContentsHandler(runtime->web_contents());
}

// static
void XWalkExtensionService::SetRegisterExtensionsCallbackForTesting(
    const RegisterExtensionsCallback& callback) {
  g_register_extensions_callback = callback;
}

void XWalkExtensionService::RegisterExtensionsForNewHost(
    content::RenderProcessHost* host) {
  ExtensionMap::iterator it = extensions_.begin();
  for (; it != extensions_.end(); ++it) {
    XWalkExtension* extension = it->second;
    host->Send(new XWalkViewMsg_RegisterExtension(
        extension->name(), extension->GetJavaScriptAPI()));
  }
}

void XWalkExtensionService::CreateWebContentsHandler(
    content::WebContents* web_contents) {
  XWalkExtensionWebContentsHandler::CreateForWebContents(web_contents);
  XWalkExtensionWebContentsHandler* handler =
      XWalkExtensionWebContentsHandler::FromWebContents(web_contents);
  handler->set_extension_service(this);
}

bool ValidateExtensionNameForTesting(const std::string& extension_name) {
  return ValidateExtensionName(extension_name);
}

}  // namespace extensions
}  // namespace xwalk
