// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/component_updater/xwalk_component_updater_configurator.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // OS_WIN
#include "build/build_config.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/update_client/configurator.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using update_client::Configurator;
using update_client::OutOfProcessPatcher;

namespace component_updater {

namespace {

// The alternative URL for the v3 protocol service endpoint.
const char kUpdaterAltUrl[] = "http://clients2.google.com/service/update2";

// The default URL for the v3 protocol service endpoint. In some cases, the
// component updater is allowed to fall back to and alternate URL source, if
// the request to the default URL source fails.
// The value of |kDefaultUrlSource| can be overridden with
// --component-updater=url-source=someurl.
const char kUpdaterDefaultUrl[] = "https://clients2.google.com/service/update2";

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater=value1,value2.
// Speed up component checking.
const char kSwitchFastUpdate[] = "fast-update";

// Add "testrequest=1" attribute to the update check request.
const char kSwitchRequestParam[] = "test-request";

// Disables pings. Pings are the requests sent to the update server that report
// the success or the failure of component install or update attempts.
extern const char kSwitchDisablePings[] = "disable-pings";

// Sets the URL for updates.
const char kSwitchUrlSource[] = "url-source";

// Disables differential updates.
const char kSwitchDisableDeltaUpdates[] = "disable-delta-updates";

#if defined(OS_WIN)
// Disables background downloads.
const char kSwitchDisableBackgroundDownloads[] = "disable-background-downloads";
#endif  // defined(OS_WIN)

// Returns true if and only if |test| is contained in |vec|.
bool HasSwitchValue(const std::vector<std::string>& vec, const char* test) {
  if (vec.empty())
    return 0;
  return (std::find(vec.begin(), vec.end(), test) != vec.end());
}

// Returns true if falling back on an alternate, unsafe, service URL is
// allowed. In the fallback case, the security of the component update relies
// only on the integrity of the CRX payloads, which is self-validating.
// This is allowed only for some of the pre-Windows Vista versions not including
// Windows XP SP3. As a side note, pings could be sent to the alternate URL too.
bool CanUseAltUrlSource() {
#if defined(OS_WIN)
  return !base::win::MaybeHasSHA256Support();
#else
  return false;
#endif  // OS_WIN
}

// If there is an element of |vec| of the form |test|=.*, returns the right-
// hand side of that assignment. Otherwise, returns an empty string.
// The right-hand side may contain additional '=' characters, allowing for
// further nesting of switch arguments.
std::string GetSwitchArgument(const std::vector<std::string>& vec,
                              const char* test) {
  if (vec.empty())
    return std::string();
  for (std::vector<std::string>::const_iterator it = vec.begin();
       it != vec.end();
       ++it) {
    const std::size_t found = it->find("=");
    if (found != std::string::npos) {
      if (it->substr(0, found) == test) {
        return it->substr(found + 1);
      }
    }
  }
  return std::string();
}

class XwalkConfigurator : public Configurator {
 public:
  XwalkConfigurator(const base::CommandLine* cmdline,
                     net::URLRequestContextGetter* url_request_getter);

  int InitialDelay() const override;
  int NextCheckDelay() override;
  int StepDelay() const override;
  int StepDelayMedium() override;
  int MinimumReCheckWait() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  size_t UrlSizeLimit() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<OutOfProcessPatcher> CreateOutOfProcessPatcher() const override;
  bool DeltasEnabled() const override;
  bool UseBackgroundDownloader() const override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetSingleThreadTaskRunner()
      const override;

 private:
  friend class base::RefCountedThreadSafe<XwalkConfigurator>;

  ~XwalkConfigurator() override {}

  net::URLRequestContextGetter* url_request_getter_;
  std::string extra_info_;
  GURL url_source_override_;
  bool fast_update_;
  bool pings_enabled_;
  bool deltas_enabled_;
  bool background_downloads_enabled_;
  bool fallback_to_alt_source_url_enabled_;
};

XwalkConfigurator::XwalkConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
    : url_request_getter_(url_request_getter),
      fast_update_(false),
      pings_enabled_(false),
      deltas_enabled_(false),
      background_downloads_enabled_(false),
      fallback_to_alt_source_url_enabled_(false) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> switch_values;
  Tokenize(cmdline->GetSwitchValueASCII(switches::kComponentUpdater),
           ",",
           &switch_values);
  fast_update_ = HasSwitchValue(switch_values, kSwitchFastUpdate);
  pings_enabled_ = !HasSwitchValue(switch_values, kSwitchDisablePings);
  deltas_enabled_ = !HasSwitchValue(switch_values, kSwitchDisableDeltaUpdates);

#if defined(OS_WIN)
  background_downloads_enabled_ =
      !HasSwitchValue(switch_values, kSwitchDisableBackgroundDownloads);
#else
  background_downloads_enabled_ = false;
#endif

  const std::string switch_url_source =
      GetSwitchArgument(switch_values, kSwitchUrlSource);
  if (!switch_url_source.empty()) {
    url_source_override_ = GURL(switch_url_source);
    DCHECK(url_source_override_.is_valid());
  }

  if (HasSwitchValue(switch_values, kSwitchRequestParam))
    extra_info_ += "testrequest=\"1\"";

  fallback_to_alt_source_url_enabled_ = CanUseAltUrlSource();
}

int XwalkConfigurator::InitialDelay() const {
  return fast_update_ ? 1 : (6 * kDelayOneMinute);
}

int XwalkConfigurator::NextCheckDelay() {
  return fast_update_ ? 3 : (6 * kDelayOneHour);
}

int XwalkConfigurator::StepDelayMedium() {
  return fast_update_ ? 3 : (15 * kDelayOneMinute);
}

int XwalkConfigurator::StepDelay() const {
  return fast_update_ ? 1 : 1;
}

int XwalkConfigurator::MinimumReCheckWait() const {
  return fast_update_ ? 30 : (6 * kDelayOneHour);
}

int XwalkConfigurator::OnDemandDelay() const {
  return fast_update_ ? 2 : (30 * kDelayOneMinute);
}

int XwalkConfigurator::UpdateDelay() const {
  return fast_update_ ? 1 : (15 * kDelayOneMinute);
}

std::vector<GURL> XwalkConfigurator::UpdateUrl() const {
  std::vector<GURL> urls;
  if (url_source_override_.is_valid()) {
    urls.push_back(GURL(url_source_override_));
  } else {
    urls.push_back(GURL(kUpdaterDefaultUrl));
    if (fallback_to_alt_source_url_enabled_) {
      urls.push_back(GURL(kUpdaterAltUrl));
    }
  }
  return urls;
}

std::vector<GURL> XwalkConfigurator::PingUrl() const {
  return pings_enabled_ ? UpdateUrl() : std::vector<GURL>();
}

base::Version XwalkConfigurator::GetBrowserVersion() const {
  return base::Version("");
}

std::string XwalkConfigurator::GetChannel() const {
 // return ChromeUpdateQueryParamsDelegate::GetChannelString();
  return "";
}

std::string XwalkConfigurator::GetLang() const {
//  return ChromeUpdateQueryParamsDelegate::GetLang();
  return "";
}

std::string XwalkConfigurator::GetOSLongName() const {
  return "";
}

std::string XwalkConfigurator::ExtraRequestParams() const {
  return extra_info_;
}

size_t XwalkConfigurator::UrlSizeLimit() const {
  return 1024ul;
}

net::URLRequestContextGetter* XwalkConfigurator::RequestContext() const {
  return url_request_getter_;
}

scoped_refptr<OutOfProcessPatcher>
XwalkConfigurator::CreateOutOfProcessPatcher() const {
//  return make_scoped_refptr(new ChromeOutOfProcessPatcher);
  return NULL;
}

bool XwalkConfigurator::DeltasEnabled() const {
  return deltas_enabled_;
}

bool XwalkConfigurator::UseBackgroundDownloader() const {
  return background_downloads_enabled_;
}

scoped_refptr<base::SequencedTaskRunner>
XwalkConfigurator::GetSequencedTaskRunner() const {
  return content::BrowserThread::GetBlockingPool()
      ->GetSequencedTaskRunnerWithShutdownBehavior(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

scoped_refptr<base::SingleThreadTaskRunner>
XwalkConfigurator::GetSingleThreadTaskRunner() const {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::FILE);
}

}  // namespace

scoped_refptr<update_client::Configurator>
MakeXwalkComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter) {
  return new XwalkConfigurator(cmdline, context_getter);
}

}  // namespace component_updater
