// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_RUNTIME_BROWSER_COMPONENT_UPDATER_WIDEVINE_CDM_COMPONENT_INSTALLER_H_
#define XWALK_RUNTIME_BROWSER_COMPONENT_UPDATER_WIDEVINE_CDM_COMPONENT_INSTALLER_H_

#include "components/component_updater/component_updater_service.h"

namespace component_updater {

class ComponentUpdateService;

// Our job is to:
// 1) Find what Widevine CDM is installed (if any).
// 2) Register with the component updater to download the latest version when
//    available.
// 3) Copy the Widevine CDM adapter bundled with chrome to the install path.
// 4) Register the Widevine CDM (via the adapter) with Chrome.
// The first part is IO intensive so we do it asynchronously in the file thread.
void RegisterWidevineCdmComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // XWALK_RUNTIME_BROWSER_COMPONENT_UPDATER_WIDEVINE_CDM_COMPONENT_INSTALLER_H_
