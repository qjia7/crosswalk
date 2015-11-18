// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_COMMON_LOGGING_CHROME_H__
#define XWALK_COMMON_LOGGING_CHROME_H__

#include "base/logging.h"
#include "base/time/time.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace logging {

// Call to initialize logging for Chrome. This sets up the chrome-specific
// logfile naming scheme and might do other things like log modules and
// setting levels in the future.
//
// The main process might want to delete any old log files on startup by
// setting delete_old_log_file, but the renderer processes should not, or
// they will delete each others' logs.
//
// XXX
// Setting suppress_error_dialogs to true disables any dialogs that would
// normally appear for assertions and crashes, and makes any catchable
// errors (namely assertions) available via GetSilencedErrorCount()
// and GetSilencedError().
void InitXwalkLogging(const base::CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file);

// Call when done using logging for Chrome.
void CleanupXwalkLogging();

// Returns the fully-qualified name of the log file.
base::FilePath GetLogFileName();

// Returns true when error/assertion dialogs are not to be shown, false
// otherwise.
bool DialogsAreSuppressed();
}  // namespace logging

#endif  // XWALK_COMMON_LOGGING_CHROME_H__
