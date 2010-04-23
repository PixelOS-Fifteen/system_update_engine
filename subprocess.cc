// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/subprocess.h"
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "chromeos/obsolete_logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

using std::string;
using std::vector;

namespace chromeos_update_engine {

void Subprocess::GChildExitedCallback(GPid pid, gint status, gpointer data) {
  COMPILE_ASSERT(sizeof(guint) == sizeof(uint32_t),
                 guint_uint32_size_mismatch);
  guint* tag = reinterpret_cast<guint*>(data);
  const SubprocessCallbackRecord& record = Get().callback_records_[*tag];
  if (record.callback)
    record.callback(status, record.callback_data);
  g_spawn_close_pid(pid);
  Get().callback_records_.erase(*tag);
  delete tag;
}

namespace {
void FreeArgv(char** argv) {
  for (int i = 0; argv[i]; i++) {
    free(argv[i]);
    argv[i] = NULL;
  }
}
}  // namespace {}

uint32_t Subprocess::Exec(const std::vector<std::string>& cmd,
                        ExecCallback callback,
                        void* p) {
  GPid child_pid;
  GError* err;
  scoped_array<char*> argv(new char*[cmd.size() + 1]);
  for (unsigned int i = 0; i < cmd.size(); i++) {
    argv[i] = strdup(cmd[i].c_str());
  }
  argv[cmd.size()] = NULL;

  scoped_array<char*> argp(new char*[2]);
  argp[0] = argp[1] = NULL;
  const char* kLdLibraryPathKey = "LD_LIBRARY_PATH";
  if (getenv(kLdLibraryPathKey)) {
    argp[0] = strdup(StringPrintf("%s=%s", kLdLibraryPathKey,
                                  getenv(kLdLibraryPathKey)).c_str());
  }

  SubprocessCallbackRecord callback_record;
  callback_record.callback = callback;
  callback_record.callback_data = p;

  bool success = g_spawn_async(NULL,  // working directory
                               argv.get(),
                               argp.get(),
                               G_SPAWN_DO_NOT_REAP_CHILD,  // flags
                               NULL,  // child setup function
                               NULL,  // child setup data pointer
                               &child_pid,
                               &err);
  FreeArgv(argv.get());
  if (!success) {
    LOG(ERROR) << "g_spawn_async failed";
    return 0;
  }
  guint* tag = new guint;
  *tag = g_child_watch_add(child_pid, GChildExitedCallback, tag);
  callback_records_[*tag] = callback_record;
  return *tag;
}

void Subprocess::CancelExec(uint32_t tag) {
  if (callback_records_[tag].callback) {
    callback_records_[tag].callback = NULL;
  }
}

bool Subprocess::SynchronousExec(const std::vector<std::string>& cmd,
                                 int* return_code) {
  GError* err = NULL;
  scoped_array<char*> argv(new char*[cmd.size() + 1]);
  for (unsigned int i = 0; i < cmd.size(); i++) {
    argv[i] = strdup(cmd[i].c_str());
  }
  argv[cmd.size()] = NULL;
  char* argp[1];
  argp[0] = NULL;

  bool success = g_spawn_sync(NULL,  // working directory
                              argv.get(),
                              argp,
                              static_cast<GSpawnFlags>(NULL),  // flags
                              NULL,  // child setup function
                              NULL,  // data for child setup function
                              NULL,  // return location for stdout
                              NULL,  // return location for stderr
                              return_code,
                              &err);
  FreeArgv(argv.get());
  if (err)
    LOG(INFO) << "err is: " << err->code << ", " << err->message;
  return success;
}

Subprocess* Subprocess::subprocess_singleton_ = NULL;

}  // namespace chromeos_update_engine
