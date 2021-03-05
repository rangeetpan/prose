// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_main.h"

// Must be before msi.h.
#include <windows.h>

#include <msi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_storage.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/current_module.h"
#include "base/win/process_startup_helper.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
<<<<<<< HEAD
#include "chrome/install_static/buildflags.h"
=======
#include "build/build_config.h"
>>>>>>> 1b569384c7ba089e4d3a190c6229ebb51791854a
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome/installer/setup/archive_patch_helper.h"
#include "chrome/installer/setup/brand_behaviors.h"
#include "chrome/installer/setup/buildflags.h"
#include "chrome/installer/setup/etw_manifest_helper.h"
#include "chrome/installer/setup/edge_register_sparse_msix.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_crash_reporting.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/launch_chrome.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_install_details.h"
#include "chrome/installer/setup/setup_singleton.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/setup_watchdog.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/setup/user_experiment.h"
#include "chrome/installer/setup/wer_report_submit.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/delete_old_versions.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING_REVIEWED) || defined(MICROSOFT_EDGE_BUILD)
#include "chrome/installer/util/google_update_util.h"
#endif

using installer::InstallerState;
using installer::InstallationState;
using installer::MasterPreferences;
using installer::ProductState;
using installer::PII_STRING;

namespace {

constexpr int kWatchdogTimeoutMinutes = 30;

const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";
const wchar_t kDisplayVersion[] = L"DisplayVersion";
const wchar_t kMsiDisplayVersionOverwriteDelay[] = L"10";  // seconds as string
const wchar_t kMsiProductIdPrefix[] = L"EnterpriseProduct";

// Overwrite an existing DisplayVersion as written by the MSI installer
// with the real version number of Chrome.
LONG OverwriteDisplayVersion(const base::string16& path,
                             const base::string16& value,
                             REGSAM wowkey) {
  base::win::RegKey key;
  LONG result = 0;
  base::string16 existing;
  if ((result = key.Open(HKEY_LOCAL_MACHINE, path.c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE | wowkey))
      != ERROR_SUCCESS) {
    VLOG(1) << "Skipping DisplayVersion update because registry key " << path
            << " does not exist in "
            << (wowkey == KEY_WOW64_64KEY ? "64" : "32") << "bit hive";
    return result;
  }
  if ((result = key.ReadValue(kDisplayVersion, &existing)) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " not found under " << path;
    return result;
  }
  if ((result = key.WriteValue(kDisplayVersion, value.c_str()))
      != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " could not be written under " << path;
    return result;
  }
  VLOG(1) << "Set DisplayVersion at " << path << " to " << value
          << " from " << existing;
  return ERROR_SUCCESS;
}

LONG OverwriteDisplayVersions(const base::string16& product,
                              const base::string16& value) {
  // The version is held in two places.  First change it in the MSI Installer
  // registry entry.  It is held under a "squashed guid" key.
  base::string16 reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\"
      L"%ls\\Products\\%ls\\InstallProperties",
      kSystemPrincipalSid, InstallUtil::GuidToSquid(product).c_str());
  LONG result1 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);

  // The display version also exists under the Uninstall registry key with
  // the original guid.  Check both WOW64_64 and WOW64_32.
  reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{%ls}",
      product.c_str());
  // Consider the operation a success if either of these succeeds.
  LONG result2 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);
  LONG result3 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_32KEY);

  return result1 != ERROR_SUCCESS ? result1 :
      (result2 != ERROR_SUCCESS ? result3 : ERROR_SUCCESS);
}

// Best effort to make MSI uninstalls sticky when updating
// after an in initial acquisition based on MSI.
void DisableMSIUninstall(const base::string16& path,
                         REGSAM wowkey) {
  base::win::RegKey key;
  base::string16 existing;
  if ((key.Open(HKEY_LOCAL_MACHINE, path.c_str(),
                KEY_QUERY_VALUE | KEY_SET_VALUE | wowkey))
      != ERROR_SUCCESS) {
    return;
  }

  key.DeleteValue(installer::kUninstallStringField);
  key.WriteValue(L"NoRemove", 1);
}

// Best effort to make MSI uninstalls sticky when updating
// after an in initial acquisition based on MSI.
void DisableMSIUninstalls(const base::string16& product) {
  // The UninstallString is held in two places.  First change it in the MSI
  // Installer  registry entry.  It is held under a "squashed guid" key.
  base::string16 reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\"
      L"%ls\\Products\\%ls\\InstallProperties",
      kSystemPrincipalSid, InstallUtil::GuidToSquid(product).c_str());
  DisableMSIUninstall(reg_path, KEY_WOW64_64KEY);

  // The UninstallString also exists under the Uninstall registry key with
  // the original guid.  Check both WOW64_64 and WOW64_32.
  reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{%ls}",
      product.c_str());
  DisableMSIUninstall(reg_path, KEY_WOW64_64KEY);
  DisableMSIUninstall(reg_path, KEY_WOW64_32KEY);
}

void DelayedOverwriteDisplayVersions(const base::FilePath& setup_exe,
                                     const std::string& id,
                                     const base::Version& version) {
  // This process has to be able to exit so we launch ourselves with
  // instructions on what to do and then return.
  base::CommandLine command_line(setup_exe);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionProduct,
                                 id);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionValue,
                                 version.GetString());
  command_line.AppendSwitchNative(installer::switches::kDelay,
                                  kMsiDisplayVersionOverwriteDelay);

  base::LaunchOptions launch_options;
  launch_options.force_breakaway_from_job_ = true;
  base::Process writer = base::LaunchProcess(command_line, launch_options);
  if (!writer.IsValid()) {
    PLOG(ERROR) << "Failed to set DisplayVersion: "
                << "could not launch subprocess to make desired changes."
                << " <<" << PII_STRING(command_line.GetCommandLineString())
                << ">>";
  }
}

// Returns NULL if no compressed archive is available for processing, otherwise
// returns a patch helper configured to uncompress and patch.
std::unique_ptr<installer::ArchivePatchHelper> CreateChromeArchiveHelper(
    const base::FilePath& setup_exe,
    const base::CommandLine& command_line,
    const installer::InstallerState& installer_state,
    const base::FilePath& working_directory,
    installer::UnPackConsumer consumer) {
  // A compressed archive is ordinarily given on the command line by the mini
  // installer. If one was not given, look for chrome.packed.7z next to the
  // running program.
  base::FilePath compressed_archive(
      command_line.GetSwitchValuePath(installer::switches::kInstallArchive));
  bool compressed_archive_specified = !compressed_archive.empty();
  if (!compressed_archive_specified) {
    compressed_archive = setup_exe.DirName().Append(
        installer::kChromeCompressedArchive);
  }

  // Fail if no compressed archive is found.
  if (!base::PathExists(compressed_archive)) {
    if (compressed_archive_specified) {
      LOG(ERROR) << installer::switches::kInstallArchive << "="
                 << PII_STRING(compressed_archive.value()) << " not found.";
    }
    return std::unique_ptr<installer::ArchivePatchHelper>();
  }

  // chrome.7z is either extracted directly from the compressed archive into the
  // working dir or is the target of patching in the working dir.
  base::FilePath target(working_directory.Append(installer::kChromeArchive));
  DCHECK(!base::PathExists(target));

  // Specify an empty path for the patch source since it isn't yet known that
  // one is needed. It will be supplied in UncompressAndPatchChromeArchive if it
  // is.
  return std::unique_ptr<installer::ArchivePatchHelper>(
      new installer::ArchivePatchHelper(working_directory, compressed_archive,
                                        base::FilePath(), target, consumer));
}

// Returns the MSI product ID from the ClientState key that is populated for MSI
// installs.  This property is encoded in a value name whose format is
// "EnterpriseProduct<GUID>" where <GUID> is the MSI product id.  <GUID> is in
// the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.  The id will be returned if
// found otherwise this method will return an empty string.
//
// This format is strange and its provenance is shrouded in mystery but it has
// the data we need, so use it.
base::string16 FindMsiProductId(const InstallerState& installer_state) {
  HKEY reg_root = installer_state.root_key();

  base::win::RegistryValueIterator value_iter(
      reg_root, install_static::GetClientStateKeyPath().c_str(),
      KEY_WOW64_32KEY);
  for (; value_iter.Valid(); ++value_iter) {
    base::string16 value_name(value_iter.Name());
    if (base::StartsWith(value_name, kMsiProductIdPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return value_name.substr(base::size(kMsiProductIdPrefix) - 1);
    }
  }
  return base::string16();
}

// Workhorse for producing an uncompressed archive (chrome.7z) given a
// chrome.packed.7z containing either a patch file based on the version of
// chrome being updated or the full uncompressed archive. Returns true on
// success, in which case |archive_type| is populated based on what was found.
// Returns false on failure, in which case |install_status| contains the error
// code and the result is written to the registry (via WriteInstallerResult).
bool UncompressAndPatchChromeArchive(
    const installer::InstallationState& original_state,
    const installer::InstallerState& installer_state,
    installer::ArchivePatchHelper* archive_helper,
    installer::ArchiveType* archive_type,
    installer::InstallStatus* install_status,
    const base::Version& previous_version) {
  installer_state.SetStage(installer::UNCOMPRESSING);

  // UMA tells us the following about the time required for uncompression as of
  // M75:
  // --- Foreground (<10%) ---
  //   Full archive: 7.5s (50%ile) / 52s (99%ile)
  //   Archive patch: <2s (50%ile) / 10-20s (99%ile)
  // --- Background (>90%) ---
  //   Full archive: 22s (50%ile) / >3m (99%ile)
  //   Archive patch: ~2s (50%ile) / 1.5m - >3m (99%ile)
  //
  // The top unpack failure result with 28 days aggregation (>=0.01%)
  // Setup.Install.LzmaUnPackResult_CompressedChromeArchive
  // 13.50% DISK_FULL
  // 0.67% ERROR_NO_SYSTEM_RESOURCES
  // 0.12% ERROR_IO_DEVICE
  // 0.05% INVALID_HANDLE
  // 0.01% INVALID_LEVEL
  // 0.01% FILE_NOT_FOUND
  // 0.01% LOCK_VIOLATION
  // 0.01% ACCESS_DENIED
  //
  // Setup.Install.LzmaUnPackResult_ChromeArchivePatch
  // 0.09% DISK_FULL
  // 0.01% FILE_NOT_FOUND
  //
  // More information can also be found with metrics:
  // Setup.Install.LzmaUnPackNTSTATUS_CompressedChromeArchive
  // Setup.Install.LzmaUnPackNTSTATUS_ChromeArchivePatch
  if (!archive_helper->Uncompress(NULL)) {
    *install_status = installer::UNCOMPRESSION_FAILED;
    installer_state.WriteInstallerResult(*install_status,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         NULL);
    return false;
  }

  // Short-circuit if uncompression produced the uncompressed archive rather
  // than a patch file.
  if (base::PathExists(archive_helper->target())) {
    *archive_type = installer::FULL_ARCHIVE_TYPE;
    return true;
  }

  // Find the installed version's archive to serve as the source for patching.
  // For Edge differential installers, we don't want the Find function to
  // fallback to use a max version folder as we have never seen that work well.
  const bool allow_max_fallback = false;
  base::FilePath patch_source(
    installer::FindArchiveToPatch(original_state, installer_state,
                                  previous_version, allow_max_fallback));
  if (patch_source.empty()) {
    LOG(ERROR) << "Failed to find archive to patch.";
    *install_status = installer::DIFF_PATCH_SOURCE_MISSING;
    installer_state.WriteInstallerResult(*install_status,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         NULL);
    return false;
  }
  archive_helper->set_patch_source(patch_source);

  // UMA tells us the following about the time required for patching as of M75:
  // --- Foreground ---
  //   12s (50%ile) / 3-6m (99%ile)
  // --- Background ---
  //   1m (50%ile) / >60m (99%ile)
  installer_state.SetStage(installer::PATCHING);
  if (!archive_helper->ApplyPatch()) {
    *install_status = installer::APPLY_DIFF_PATCH_FAILED;
    installer_state.WriteInstallerResult(
        *install_status, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, NULL);
    return false;
  }

  *archive_type = installer::INCREMENTAL_ARCHIVE_TYPE;
  return true;
}

// Repetitively attempts to delete all files that belong to old versions of
// Chrome from |install_dir|. Waits 15 seconds before the first attempt and 5
// minutes after each unsuccessful attempt. Returns when no files that belong to
// an old version of Chrome remain or when another process tries to acquire the
// SetupSingleton.
installer::InstallStatus RepeatDeleteOldVersions(
    const base::FilePath& install_dir,
    const installer::SetupSingleton& setup_singleton) {
  // The 99th percentile of the number of attempts it takes to successfully
  // delete old versions is 2.75. The 75th percentile is 1.77. 98% of calls to
  // this function will successfully delete old versions.
  // Source: 30 days of UMA data on June 25, 2019.
  constexpr int kMaxNumAttempts = 3;
  int num_attempts = 0;

  while (num_attempts < kMaxNumAttempts) {
    // Wait 15 seconds before the first attempt because trying to delete old
    // files right away is likely to fail. Indeed, this is called in 2
    // occasions:
    // - When the installer fails to delete old files after a not-in-use update:
    //   retrying immediately is likely to fail again.
    // - When executables are successfully renamed on Chrome startup or
    //   shutdown: old files can't be deleted because Chrome is still in use.
    // Wait 5 minutes after an unsuccessful attempt because retrying immediately
    // is likely to fail again.
    const base::TimeDelta max_wait_time = num_attempts == 0
                                              ? base::TimeDelta::FromSeconds(15)
                                              : base::TimeDelta::FromMinutes(5);
    if (setup_singleton.WaitForInterrupt(max_wait_time)) {
      VLOG(1) << "Exiting --delete-old-versions process because another "
                 "process tries to acquire the SetupSingleton.";
      return installer::SETUP_SINGLETON_RELEASED;
    }

    const bool priority_was_changed_to_background =
        base::Process::Current().SetProcessBackgrounded(true);
    const bool delete_old_versions_success =
        installer::DeleteOldVersions(install_dir);
    if (priority_was_changed_to_background)
      base::Process::Current().SetProcessBackgrounded(false);
    ++num_attempts;

    if (delete_old_versions_success) {
      VLOG(1) << "Successfully deleted all old files from "
                 "--delete-old-versions process.";
      return installer::DELETE_OLD_VERSIONS_SUCCESS;
    } else if (num_attempts == 1) {
      VLOG(1) << "Failed to delete all old files from --delete-old-versions "
                 "process. Will retry every five minutes.";
    }
  }

#if defined(MICROSOFT_EDGE_BUILD)
  VLOG(1) << "Try to schedule deletion of old versions";
  if (installer::UpdateOldVersionsCleanupTask(true)) {
    VLOG(1) << "Successfully scheduled deletion of all old files from "
               "--delete-old-versions process.";
    // Note that we are using the same histogram name, the num_attempts
    // of kMaxNumAttempts will tell us it is a scheduled deletion.
    UMA_HISTOGRAM_COUNTS_100(
        "Setup.Install.NumDeleteOldVersionsAttemptsBeforeSuccess",
        num_attempts);
    return installer::DELETE_OLD_VERSIONS_SUCCESS;
  }
#endif

  VLOG(1) << "Exiting --delete-old-versions process after retrying too many "
             "times to delete all old files.";
  DCHECK_EQ(num_attempts, kMaxNumAttempts);
  return installer::DELETE_OLD_VERSIONS_TOO_MANY_ATTEMPTS;
}

// This function is called when --rename-msedge-exe option is specified on
// setup.exe command line. This function assumes an in-use update has happened
// for Edge so there should be files called new_msedge.exe and
// new_edge_proxy.exe on the file system and a key called 'opv' in the
// registry. This function will move new_msedge.exe to msedge.exe,
// new_edge_proxy.exe to msedge_proxy.exe and delete 'opv' key in one atomic
// operation. This function also deletes elevation policies associated with the
// old version if they exist. |setup_exe| is the path to the current executable.
installer::InstallStatus RenameChromeExecutables(
    const base::FilePath& setup_exe,
    const InstallationState& original_state,
    InstallerState* installer_state) {
  const base::FilePath &target_path = installer_state->target_path();
  base::FilePath chrome_exe(target_path.Append(installer::kChromeExe));
  base::FilePath chrome_new_exe(target_path.Append(installer::kChromeNewExe));
  base::FilePath chrome_old_exe(target_path.Append(installer::kChromeOldExe));
  base::FilePath chrome_proxy_exe(
      target_path.Append(installer::kChromeProxyExe));
  base::FilePath chrome_proxy_new_exe(
      target_path.Append(installer::kChromeProxyNewExe));
  base::FilePath chrome_proxy_old_exe(
      target_path.Append(installer::kChromeProxyOldExe));

  base::FilePath pwahelper_exe(target_path.Append(installer::kPwaHelperExe));
  base::FilePath pwahelper_new_exe(
      target_path.Append(installer::kPwaHelperNewExe));
  base::FilePath pwahelper_old_exe(
      target_path.Append(installer::kPwaHelperOldExe));

  // Create a temporary backup directory on the same volume as chrome.exe so
  // that moving in-use files doesn't lead to trouble.
  installer::SelfCleaningTempDir temp_path;
  if (!temp_path.Initialize(target_path.DirName(),
                            installer::kInstallTempDir)) {
    PLOG(ERROR) << "Failed to create Temp directory "
                << PII_STRING(target_path.DirName()
                       .Append(installer::kInstallTempDir).value());
    return installer::RENAME_FAILED;
  }
  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  // Move chrome.exe to old_chrome.exe, then move new_chrome.exe to chrome.exe.
  install_list->AddMoveTreeWorkItem(chrome_exe.value(),
                                    chrome_old_exe.value(),
                                    temp_path.path().value(),
                                    WorkItem::ALWAYS_MOVE);
  install_list->AddMoveTreeWorkItem(chrome_new_exe.value(),
                                    chrome_exe.value(),
                                    temp_path.path().value(),
                                    WorkItem::ALWAYS_MOVE);
  install_list->AddDeleteTreeWorkItem(chrome_new_exe, temp_path.path());

  // Move chrome_proxy.exe to old_chrome_proxy.exe if it exists (a previous
  // installation may not have included it), then move new_chrome_proxy.exe to
  // chrome_proxy.exe.
  std::unique_ptr<WorkItemList> existing_proxy_rename_list(
      WorkItem::CreateConditionalWorkItemList(
          new ConditionRunIfFileExists(chrome_proxy_exe)));
  existing_proxy_rename_list->set_log_message("ExistingProxyRenameItemList");
  existing_proxy_rename_list->AddMoveTreeWorkItem(
      chrome_proxy_exe.value(), chrome_proxy_old_exe.value(),
      temp_path.path().value(), WorkItem::ALWAYS_MOVE);
  install_list->AddWorkItem(existing_proxy_rename_list.release());
  install_list->AddMoveTreeWorkItem(
      chrome_proxy_new_exe.value(), chrome_proxy_exe.value(),
      temp_path.path().value(), WorkItem::ALWAYS_MOVE);
  install_list->AddDeleteTreeWorkItem(chrome_proxy_new_exe, temp_path.path());

  std::unique_ptr<WorkItemList> existing_pwahelper_rename_list(
      WorkItem::CreateConditionalWorkItemList(
          new ConditionRunIfFileExists(pwahelper_exe)));
  existing_pwahelper_rename_list->set_log_message(
      "ExistingPwaHelperRenameItemList");
  existing_pwahelper_rename_list->AddMoveTreeWorkItem(
      pwahelper_exe.value(), pwahelper_old_exe.value(),
      temp_path.path().value(), WorkItem::ALWAYS_MOVE);
  install_list->AddWorkItem(existing_pwahelper_rename_list.release());
  install_list->AddMoveTreeWorkItem(
      pwahelper_new_exe.value(), pwahelper_exe.value(),
      temp_path.path().value(), WorkItem::ALWAYS_MOVE);
  install_list->AddDeleteTreeWorkItem(pwahelper_new_exe, temp_path.path());

  // Add work items to delete Chrome's "opv", "cpv", and "cmd" values.
  // TODO(grt): Clean this up; https://crbug.com/577816.
  HKEY reg_root = installer_state->root_key();
  const base::string16 clients_key = install_static::GetClientsKeyPath();
  install_list->AddDeleteRegValueWorkItem(reg_root, clients_key,
                                          KEY_WOW64_32KEY,
                                          google_update::kRegOldVersionField);
  install_list->AddDeleteRegValueWorkItem(
      reg_root, clients_key, KEY_WOW64_32KEY,
      google_update::kRegCriticalVersionField);
  install_list->AddDeleteRegValueWorkItem(reg_root, clients_key,
                                          KEY_WOW64_32KEY,
                                          google_update::kRegRenameCmdField);
  // old_chrome.exe is still in use in most cases, so ignore failures here.
  install_list->AddDeleteTreeWorkItem(chrome_old_exe, temp_path.path())
      ->set_best_effort(true);
  install_list->AddDeleteTreeWorkItem(chrome_proxy_old_exe, temp_path.path())
      ->set_best_effort(true);
  install_list->AddDeleteTreeWorkItem(pwahelper_old_exe, temp_path.path())
      ->set_best_effort(true);

  installer::InstallStatus ret = installer::RENAME_SUCCESSFUL;
  if (!install_list->Do()) {
    LOG(ERROR) << "Renaming of executables failed. Rolling back any changes.";
    install_list->Rollback();
    ret = installer::RENAME_FAILED;
  }
  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory "
          << PII_STRING(temp_path.path().value());

  return ret;
}

// Checks for compatibility between the current state of the system and the
// desired operation.
// Also blocks simultaneous user-level and system-level installs.  In the case
// of trying to install user-level Chrome when system-level exists, the
// existing system-level Chrome is launched.
// When the pre-install conditions are not satisfied, the result is written to
// the registry (via WriteInstallerResult), |status| is set appropriately, and
// false is returned.
bool CheckPreInstallConditions(const InstallationState& original_state,
                               const InstallerState& installer_state,
                               installer::InstallStatus* status) {
  if (!installer_state.system_install()) {
    // This is a user-level installation. Make sure that we are not installing
    // on top of an existing system-level installation.

    const ProductState* user_level_product_state =
        original_state.GetProductState(false);
    const ProductState* system_level_product_state =
        original_state.GetProductState(true);

    // Allow upgrades to proceed so that out-of-date versions are not left
    // around.
    if (user_level_product_state)
      return true;

    // This is a new user-level install...

    if (system_level_product_state) {
      // ... and the product already exists at system-level.
      LOG(ERROR) << "Already installed version "
                 << system_level_product_state->version().GetString()
                 << " at system-level conflicts with this one at user-level.";
      // Instruct Google Update to launch the existing system-level Chrome.
      // There should be no error dialog.
      base::FilePath install_path(
          installer::GetChromeInstallPath(true /* system_install */));
      if (install_path.empty()) {
        // Give up if we failed to construct the install path.
        *status = installer::OS_ERROR;
        installer_state.WriteInstallerResult(*status, IDS_INSTALL_OS_ERROR_BASE,
                                             nullptr);
      } else {
        int existing_version_launched_resource_id =
            IDS_INSTALL_EXISTING_VERSION_LAUNCHED_BASE;
        if (installer_state.is_browser()) {
          *status = installer::EXISTING_VERSION_LAUNCHED;
          base::FilePath chrome_exe =
              install_path.Append(installer::kChromeExe);
          base::CommandLine cmd(chrome_exe);
          cmd.AppendSwitch(switches::kForceFirstRun);
          VLOG(1) << "Launching existing system-level Edge instead.";
          base::LaunchProcess(cmd, base::LaunchOptions());
        } else if (installer_state.is_webview()) {
          *status = installer::SYSTEM_LEVEL_INSTALL_EXISTS;
          existing_version_launched_resource_id =
              IDS_WEBVIEW_INSTALL_EXISTING_VERSION_LAUNCHED_BASE;
        }
        installer_state.WriteInstallerResult(
            *status, existing_version_launched_resource_id, nullptr);
      }
      return false;
    }
  }

  return true;
}

// Initializes |temp_path| to "Temp" within the target directory, and
// |unpack_path| to a random directory beginning with "source" within
// |temp_path|. Returns false on error.
bool CreateTemporaryAndUnpackDirectories(
    const InstallerState& installer_state,
    installer::SelfCleaningTempDir* temp_path,
    base::FilePath* unpack_path) {
  DCHECK(temp_path && unpack_path);

  if (!temp_path->Initialize(installer_state.target_path().DirName(),
                             installer::kInstallTempDir)) {
    PLOG(ERROR) << "Could not create temporary path.";
    return false;
  }
  VLOG(1) << "Created path " << PII_STRING(temp_path->path().value());

  if (!base::CreateTemporaryDirInDir(temp_path->path(),
                                     installer::kInstallSourceDir,
                                     unpack_path)) {
    PLOG(ERROR) << "Could not create temporary path for unpacked archive.";
    return false;
  }

  return true;
}

installer::InstallStatus UninstallProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    bool remove_all,
    bool force_uninstall) {
  const ProductState* product_state =
      original_state.GetProductState(installer_state.system_install());
  if (product_state != NULL) {
    VLOG(1) << "version on the system: "
            << product_state->version().GetString();
  } else if (!force_uninstall) {
    LOG(ERROR) << "Edge not found for uninstall.";
    return installer::CHROME_NOT_INSTALLED;
  }

  return installer::UninstallProduct(original_state, installer_state, setup_exe,
                                     remove_all, force_uninstall, cmd_line);
}

installer::InstallStatus UninstallProducts(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line) {
  // System-level Chrome will be launched via this command if its program gets
  // set below.
  base::CommandLine system_level_cmd(base::CommandLine::NO_PROGRAM);

  if (installer_state.is_browser() &&
      cmd_line.HasSwitch(installer::switches::kSelfDestruct) &&
      !installer_state.system_install()) {
    const base::FilePath system_exe_path(
        installer::GetChromeInstallPath(true).Append(installer::kChromeExe));
    system_level_cmd.SetProgram(system_exe_path);
  }

  installer::InstallStatus install_status = installer::UNINSTALL_SUCCESSFUL;
  const bool force = cmd_line.HasSwitch(installer::switches::kForceUninstall);
  const bool remove_all = !cmd_line.HasSwitch(
      installer::switches::kDoNotRemoveSharedItems);

  install_status = UninstallProduct(original_state, installer_state, setup_exe,
                                    cmd_line, remove_all, force);

  installer::CleanUpInstallationDirectoryAfterUninstall(
      original_state, installer_state, setup_exe, &install_status);

  // The app and vendor dirs may now be empty. Make a last-ditch attempt to
  // delete them.
  installer::DeleteChromeDirectoriesIfEmpty(installer_state.target_path());

  // Trigger Active Setup if it was requested for the chrome product. This needs
  // to be done after the UninstallProduct calls as some of them might
  // otherwise terminate the process launched by TriggerActiveSetupCommand().
  if (cmd_line.HasSwitch(installer::switches::kTriggerActiveSetup))
    InstallUtil::TriggerActiveSetupCommand();

  if (!system_level_cmd.GetProgram().empty())
    base::LaunchProcess(system_level_cmd, base::LaunchOptions());

  // Tell Google Update that an uninstall has taken place if this install did
  // not originate from the MSI. Google Update has its own logic relating to
  // MSI-driven uninstalls that conflicts with this. Ignore the return value:
  // success or failure of Google Update has no bearing on the success or
  // failure of Chrome's uninstallation.
  if (install_static::IsEnabledGoogleUpdateIntegration()) {
    if (!installer_state.is_msi()) {
      google_update::UninstallGoogleUpdate(installer_state.system_install());
    }
  }

  return install_status;
}

installer::InstallStatus InstallProducts(
    const InstallationState& original_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const MasterPreferences& prefs,
    InstallerState* installer_state,
    base::FilePath* installer_directory,
    base::Version* installer_version_returned) {
  DCHECK(installer_state);
  base::TimeTicks install_start_time = base::TimeTicks::Now();
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  installer::ArchiveType archive_type = installer::UNKNOWN_ARCHIVE_TYPE;
  installer_state->SetStage(installer::PRECONDITIONS);
  // Remove any legacy "-stage:*" values from the product's "ap" value.
  installer::UpdateInstallStatus(archive_type, install_status);

  // Drop to background processing mode if the process was started below the
  // normal process priority class. This is done here because InstallProducts-
  // Helper has read-only access to the state and because the action also
  // affects everything else that runs below.
  const bool entered_background_mode = installer::AdjustProcessPriority();
  installer_state->set_background_mode(entered_background_mode);
  VLOG_IF(1, entered_background_mode) << "Entered background processing mode.";

  if (CheckPreInstallConditions(original_state, *installer_state,
                                &install_status)) {
    VLOG(1) << "Installing to "
            << PII_STRING(installer_state->target_path().value());
    install_status = InstallProductsHelper(
        original_state, setup_exe, cmd_line, prefs, *installer_state,
        installer_directory, &archive_type, installer_version_returned);
  } else {
    // CheckPreInstallConditions must set the status on failure.
    DCHECK_NE(install_status, installer::UNKNOWN_STATUS);
  }

  // Delete the master preferences file if present. Note that we do not care
  // about rollback here and we schedule for deletion on reboot if the delete
  // fails. As such, we do not use DeleteTreeWorkItem.
  if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
    base::FilePath prefs_path(cmd_line.GetSwitchValuePath(
        installer::switches::kInstallerData));
    if (!base::DeleteFile(prefs_path, false)) {
      LOG(ERROR) << "Failed deleting master preferences file "
                 << PII_STRING(prefs_path.value())
                 << ", scheduling for deletion after reboot.";
      ScheduleFileSystemEntityForDeletion(prefs_path);
    }
  }

  installer::UpdateInstallStatus(archive_type, install_status);

  base::TimeDelta install_elapsed_time =
      base::TimeTicks::Now() - install_start_time;
  if (entered_background_mode) {
    UmaHistogramLongTimes("Microsoft.Setup.Install.InstallTime.background",
                          install_elapsed_time);
  } else {
    UmaHistogramLongTimes("Microsoft.Setup.Install.InstallTime",
                          install_elapsed_time);
  }
  VLOG(1) << "Milliseconds spent on install: "
          << install_elapsed_time.InMilliseconds()
          << ". Is background: " << entered_background_mode;

  return install_status;
}

installer::InstallStatus ShowEulaDialog(const base::string16& inner_frame) {
  VLOG(1) << "About to show EULA";
  base::string16 eula_path = installer::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer::EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  installer::EulaHTMLDialog dlg(eula_path, inner_frame);
  installer::EulaHTMLDialog::Outcome outcome = dlg.ShowModal();
  if (installer::EulaHTMLDialog::REJECTED == outcome) {
    LOG(ERROR) << "EULA rejected or EULA failure";
    return installer::EULA_REJECTED;
  }
  if (installer::EulaHTMLDialog::ACCEPTED_OPT_IN == outcome) {
    VLOG(1) << "EULA accepted (opt-in)";
    return installer::EULA_ACCEPTED_OPT_IN;
  }
  VLOG(1) << "EULA accepted (no opt-in)";
  return installer::EULA_ACCEPTED;
}

// Creates the sentinel indicating that the EULA was required and has been
// accepted.
bool CreateEulaSentinel() {
  base::FilePath eula_sentinel;
  if (!InstallUtil::GetEulaSentinelFilePath(&eula_sentinel))
    return false;

  return (base::CreateDirectory(eula_sentinel.DirName()) &&
          base::WriteFile(eula_sentinel, "", 0) != -1);
}

installer::InstallStatus RegisterDevChrome(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line) {
  // Only proceed with registering a dev chrome if no real Chrome installation
  // of the same install mode is present on this system.
  const ProductState* existing_chrome = original_state.GetProductState(false);
  if (!existing_chrome)
    existing_chrome = original_state.GetProductState(true);
  if (existing_chrome) {
    static const wchar_t kPleaseUninstallYourChromeMessage[] =
        L"You already have a full-installation (non-dev) of %1ls, please "
        L"uninstall it first using Add/Remove Programs in the control panel.";
    base::string16 name(InstallUtil::GetDisplayName());
    base::string16 message(
        base::StringPrintf(kPleaseUninstallYourChromeMessage, name.c_str()));

    LOG(ERROR) << "Aborting operation: another installation of " << name
               << " was found, as a last resort (if the product is not present "
                  "in Add/Remove Programs), try executing: "
               << PII_STRING(existing_chrome->uninstall_command()
                                .GetCommandLineString());
    MessageBox(NULL, message.c_str(), NULL, MB_ICONERROR);
    return installer::INSTALL_FAILED;
  }

  base::FilePath chrome_exe(
      cmd_line.GetSwitchValuePath(installer::switches::kRegisterDevChrome));
  if (chrome_exe.empty())
    chrome_exe = setup_exe.DirName().Append(installer::kChromeExe);
  if (!chrome_exe.IsAbsolute())
    chrome_exe = base::MakeAbsoluteFilePath(chrome_exe);

  installer::InstallStatus status = installer::FIRST_INSTALL_SUCCESS;
  if (base::PathExists(chrome_exe)) {
    // Create the Start menu shortcut and pin it to the Win7+ taskbar.
    ShellUtil::ShortcutProperties shortcut_properties(ShellUtil::CURRENT_USER);
    ShellUtil::AddDefaultShortcutProperties(chrome_exe, &shortcut_properties);
    shortcut_properties.set_pin_to_taskbar(true);
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, shortcut_properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS);

    // Register Chrome at user-level and make it default.
    if (ShellUtil::CanMakeChromeDefaultUnattended()) {
      ShellUtil::MakeChromeDefault(ShellUtil::CURRENT_USER, chrome_exe, true);
    } else {
      ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_exe);
    }
  } else {
    LOG(ERROR) << "Path not found: " << PII_STRING(chrome_exe.value());
    status = installer::INSTALL_FAILED;
  }
  return status;
}

// This method processes any command line options that make setup.exe do
// various tasks other than installation (renaming chrome.exe, showing eula
// among others). This function returns true if any such command line option
// has been found and processed (so setup.exe should exit at that point).
bool HandleNonInstallCmdLineOptions(const base::FilePath& setup_exe,
                                    const base::CommandLine& cmd_line,
                                    InstallationState* original_state,
                                    InstallerState* installer_state,
                                    int* exit_code) {
  // This option is independent of all others so doesn't belong in the if/else
  // block below.
  if (cmd_line.HasSwitch(installer::switches::kDelay)) {
    const std::string delay_seconds_string(
        cmd_line.GetSwitchValueASCII(installer::switches::kDelay));
    int delay_seconds;
    if (base::StringToInt(delay_seconds_string, &delay_seconds) &&
        delay_seconds > 0) {
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(delay_seconds));
    }
  }

  // TODO(gab): Add a local |status| variable which each block below sets;
  // only determine the |exit_code| from |status| at the end (this will allow
  // this method to validate that
  // (!handled || status != installer::UNKNOWN_STATUS)).
  bool handled = true;
  // TODO(tommi): Split these checks up into functions and use a data driven
  // map of switch->function.
  if (cmd_line.HasSwitch(installer::switches::kUpdateSetupExe)) {
    installer_state->SetStage(installer::UPDATING_SETUP);
    installer::InstallStatus status = installer::SETUP_PATCH_FAILED;
    // If --update-setup-exe command line option is given, we apply the given
    // patch to current exe, and store the resulting binary in the path
    // specified by --new-setup-exe. But we need to first unpack the file
    // given in --update-setup-exe.
    base::ScopedTempDir temp_path;
    if (!temp_path.CreateUniqueTempDir()) {
      PLOG(ERROR) << "Could not create temporary path.";
    } else {
      base::FilePath compressed_archive(cmd_line.GetSwitchValuePath(
          installer::switches::kUpdateSetupExe));
      VLOG(1) << "Opening archive " << PII_STRING(compressed_archive.value());
      // The top unpack failure result with 28 days aggregation (>=0.01%)
      // Setup.Install.LzmaUnPackResult_SetupExePatch
      // 0.02% PATH_NOT_FOUND
      //
      // More information can also be found with metric:
      // Setup.Install.LzmaUnPackNTSTATUS_SetupExePatch
      if (installer::ArchivePatchHelper::UncompressAndPatch(
              temp_path.GetPath(), compressed_archive, setup_exe,
              cmd_line.GetSwitchValuePath(installer::switches::kNewSetupExe),
              installer::UnPackConsumer::SETUP_EXE_PATCH)) {
        status = installer::NEW_VERSION_UPDATED;
      }
      if (!temp_path.Delete()) {
        // PLOG would be nice, but Delete() doesn't leave a meaningful value in
        // the Windows last-error code.
        LOG(WARNING) << "Scheduling temporary path "
                     << PII_STRING(temp_path.GetPath().value())
                     << " for deletion at reboot.";
        ScheduleDirectoryForDeletion(temp_path.GetPath());
      }
    }

    *exit_code = InstallUtil::GetInstallReturnCode(status);
    if (*exit_code) {
      LOG(WARNING) << "setup.exe patching failed.";
      installer_state->WriteInstallerResult(
          status, IDS_SETUP_PATCH_FAILED_BASE, NULL);
    }
  } else if (cmd_line.HasSwitch(installer::switches::kShowEula)) {
    // Check if we need to show the EULA. If it is passed as a command line
    // then the dialog is shown and regardless of the outcome setup exits here.
    base::string16 inner_frame =
        cmd_line.GetSwitchValueNative(installer::switches::kShowEula);
    *exit_code = ShowEulaDialog(inner_frame);

    if (installer::EULA_REJECTED != *exit_code) {
      if (GoogleUpdateSettings::SetEulaConsent(true))
        CreateEulaSentinel();
    }
  } else if (cmd_line.HasSwitch(installer::switches::kConfigureUserSettings)) {
    // NOTE: Should the work done here, on kConfigureUserSettings, change:
    // kActiveSetupVersion in install_worker.cc needs to be increased for Active
    // Setup to invoke this again for all users of this install.
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    if (installer_state->system_install()) {
      if (installer_state->is_browser()) {
        bool force =
            cmd_line.HasSwitch(installer::switches::kForceConfigureUserSettings);
        installer::HandleActiveSetupForBrowser(*installer_state, force);
        status = installer::INSTALL_REPAIRED;
      } else {
        status = installer::INVALID_USAGE_WEBVIEW;
      }
    } else {
      LOG(DFATAL)
          << "--configure-user-settings is incompatible with user-level";
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterDevChrome)) {
    installer::InstallStatus status = RegisterDevChrome(
        *original_state, *installer_state, setup_exe, cmd_line);
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser)) {
    installer::InstallStatus status = installer::UNKNOWN_STATUS;
    // If --register-msedge-browser option is specified, register all Chrome
    // protocol/file associations, as well as register it as a valid browser for
    // Start Menu->Internet shortcut. This switch will also register Chrome as a
    // valid handler for a set of URL protocols that Chrome may become the
    // default handler for, either by the user marking Chrome as the default
    // browser, through the Windows Default Programs control panel settings, or
    // through website use of registerProtocolHandler. These protocols are found
    // in ShellUtil::kPotentialProtocolAssociations.  The
    // --register-url-protocol will additionally register Chrome as a potential
    // handler for the supplied protocol, and is used if a website registers a
    // handler for a protocol not found in
    // ShellUtil::kPotentialProtocolAssociations.  These options should only be
    // used when setup.exe is launched with admin rights. We do not make any
    // user specific changes with this option.
    DCHECK(IsUserAnAdmin());
    base::FilePath chrome_exe(cmd_line.GetSwitchValuePath(
        installer::switches::kRegisterChromeBrowser));
    base::string16 suffix;
    if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    if (cmd_line.HasSwitch(installer::switches::kRegisterURLProtocol)) {
      base::string16 protocol = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterURLProtocol);
      // ShellUtil::RegisterChromeForProtocol performs all registration
      // done by ShellUtil::RegisterChromeBrowser, as well as registering
      // with Windows as capable of handling the supplied protocol.
      if (ShellUtil::RegisterChromeForProtocol(chrome_exe, suffix, protocol,
                                               false))
        status = installer::IN_USE_UPDATED;
    } else {
      if (ShellUtil::RegisterChromeBrowser(chrome_exe, suffix, false))
        status = installer::IN_USE_UPDATED;
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kDeleteOldVersions) ||
             cmd_line.HasSwitch(installer::switches::kRenameChromeExe)) {
    std::unique_ptr<installer::SetupSingleton> setup_singleton(
        installer::SetupSingleton::Acquire(
            cmd_line, MasterPreferences::ForCurrentProcess(), original_state,
            installer_state));
    if (!setup_singleton) {
      *exit_code = installer::SETUP_SINGLETON_ACQUISITION_FAILED;
    } else if (cmd_line.HasSwitch(installer::switches::kDeleteOldVersions)) {
      *exit_code = RepeatDeleteOldVersions(installer_state->target_path(),
                                           *setup_singleton);
    } else {
      DCHECK(cmd_line.HasSwitch(installer::switches::kRenameChromeExe));
      *exit_code =
          RenameChromeExecutables(setup_exe, *original_state, installer_state);

      bool rename_success = false;
      if (*exit_code == installer::RENAME_SUCCESSFUL) {
        rename_success = true;
      }
      UMA_HISTOGRAM_BOOLEAN("Microsoft.Setup.Install.RenameExeSuccess",
                            rename_success);
    }

  } else if (cmd_line.HasSwitch(
                 installer::switches::kRemoveChromeRegistration)) {
    // This is almost reverse of --register-msedge-browser option above.
    // Here we delete Chrome browser registration. This option should only
    // be used when setup.exe is launched with admin rights. We do not
    // make any user specific changes in this option.
    base::string16 suffix;
    if (cmd_line.HasSwitch(
            installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    installer::InstallStatus tmp = installer::UNKNOWN_STATUS;
    installer::DeleteChromeRegistrationKeys(*installer_state,
                                            HKEY_LOCAL_MACHINE, suffix, &tmp);
    *exit_code = tmp;
  } else if (cmd_line.HasSwitch(installer::switches::kOnOsUpgrade)) {
    installer::InstallStatus status;
    if (installer_state->is_browser()) {
      status = installer::INVALID_STATE_FOR_OPTION;
      std::unique_ptr<FileVersionInfo> version_info(
          FileVersionInfo::CreateFileVersionInfo(setup_exe));
      const base::Version installed_version(
          base::UTF16ToUTF8(version_info->product_version()));
      if (installed_version.IsValid()) {
        installer::HandleOsUpgradeForBrowser(*installer_state,
            installed_version, setup_exe);
        status = installer::INSTALL_REPAIRED;
      } else {
        LOG(DFATAL) << "Failed to extract product version from "
                    << setup_exe.value();
      }
    } else {
        status = installer::INVALID_USAGE_WEBVIEW;
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
#if defined(MICROSOFT_EDGE_BUILD)
  } else if (cmd_line.HasSwitch(installer::switches::kBrowserReplacement)) {
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    std::unique_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(setup_exe));
    const base::Version installed_version(
        base::UTF16ToUTF8(version_info->product_version()));

    if (installed_version.IsValid() &&
        installer::HandleFinishBrowserReplacement(*installer_state, setup_exe,
                                                  installed_version)) {
      status = installer::INSTALL_REPAIRED;
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
#endif  // defined(MICROSOFT_EDGE_BUILD)
  } else if (cmd_line.HasSwitch(installer::switches::kUserExperiment)) {
    installer::RunUserExperiment(cmd_line,
                                 MasterPreferences::ForCurrentProcess(),
                                 original_state, installer_state);
    exit_code = 0;
  } else if (cmd_line.HasSwitch(installer::switches::kPatch)) {
    const std::string patch_type_str(
        cmd_line.GetSwitchValueASCII(installer::switches::kPatch));
    const base::FilePath input_file(
        cmd_line.GetSwitchValuePath(installer::switches::kInputFile));
    const base::FilePath patch_file(
        cmd_line.GetSwitchValuePath(installer::switches::kPatchFile));
    const base::FilePath output_file(
        cmd_line.GetSwitchValuePath(installer::switches::kOutputFile));

    if (patch_type_str == installer::kCourgette) {
      *exit_code = installer::CourgettePatchFiles(input_file,
                                                  patch_file,
                                                  output_file);
    } else if (patch_type_str == installer::kBsdiff) {
      *exit_code = installer::BsdiffPatchFiles(input_file,
                                               patch_file,
                                               output_file);
#if BUILDFLAG(ZUCCHINI)
    } else if (patch_type_str == installer::kZucchini) {
      *exit_code =
          installer::ZucchiniPatchFiles(input_file, patch_file, output_file);
#endif  // BUILDFLAG(ZUCCHINI)
    } else {
      *exit_code = installer::PATCH_INVALID_ARGUMENTS;
    }
  } else if (cmd_line.HasSwitch(installer::switches::kReenableAutoupdates)) {
    // setup.exe has been asked to attempt to reenable updates for Chrome.
    bool updates_enabled = GoogleUpdateSettings::ReenableAutoupdates();
    *exit_code = updates_enabled ? installer::REENABLE_UPDATES_SUCCEEDED :
                                   installer::REENABLE_UPDATES_FAILED;
  } else if (cmd_line.HasSwitch(
                 installer::switches::kSetDisplayVersionProduct)) {
    const base::string16 registry_product(
        cmd_line.GetSwitchValueNative(
            installer::switches::kSetDisplayVersionProduct));
    const base::string16 registry_value(
        cmd_line.GetSwitchValueNative(
            installer::switches::kSetDisplayVersionValue));
    *exit_code = OverwriteDisplayVersions(registry_product, registry_value);
#if EXCLUDED_FROM_EDGE
  } else if (cmd_line.HasSwitch(installer::switches::kStoreDMToken)) {
    // Write the specified token to the registry, overwriting any already
    // existing value.
    base::string16 token_switch_value =
        cmd_line.GetSwitchValueNative(installer::switches::kStoreDMToken);
    base::Optional<std::string> token;
    if (!(token = installer::DecodeDMTokenSwitchValue(token_switch_value)) ||
        !installer::StoreDMToken(*token)) {
      *exit_code = installer::STORE_DMTOKEN_FAILED;
    } else {
      *exit_code = installer::STORE_DMTOKEN_SUCCESS;
    }
#endif  // EXCLUDED_FROM_EDGE
  } else if (cmd_line.HasSwitch(
      installer::switches::kRegisterPackageIdentity)) {
    installer::SparseMSIX::RegisterPackageIdentity(*installer_state);
  } else if (cmd_line.HasSwitch(installer::switches::kLaunchAfterUnlock)) {
    // Do not take the exclusive mutex. Similar to how active setup works,
    // we want any other setup operation to run while we are waiting.
    auto_launch_util::WaitForSessionUnlock();
    if (!InstallUtil::IsFirstRunSentinelPresent()) {
      const auto launch_behavior =
          auto_launch_util::GetActiveSetupAutoLaunchBehavior();
      base::CommandLine launch_cmd =
          auto_launch_util::CreateActiveSetupAutoLaunchCommand(launch_behavior);
      base::LaunchOptions launch_options =
          auto_launch_util::GetActiveSetupAutoLaunchOptions(launch_behavior);
      VLOG(1) << "Launching the browser after unlock. Behavior: "
              << static_cast<int>(launch_behavior) << " Arguments: "
              << PII_STRING(launch_cmd.GetCommandLineString());
      installer::LaunchChromeBrowser(installer_state->target_path(), launch_cmd,
                                    launch_options);
    } else {
      // The browser was launched some other way while we were waiting. don't
      // launch it again.
      VLOG(1) << "The browser will not be auto-launched. It ran once already.";
    }
    *exit_code = 0;
  } else {
    handled = false;
  }

  // If we did a successful rename, launch process to delete old versions here
  // after the setup singleton is released so that this launched process can
  // reliably acquire it.
  if (*exit_code == installer::RENAME_SUCCESSFUL) {
    installer::LaunchDeleteOldVersionsProcess(setup_exe, *installer_state);
  }

  // Run package identity registration on OS upgrade or version update
  if (*exit_code == installer::INSTALL_REPAIRED ||
      *exit_code == installer::RENAME_SUCCESSFUL) {
    installer::LaunchRegisterPackageIdentityProcess(setup_exe,
      *installer_state);
  }

  return handled;
}

}  // namespace

namespace installer {

InstallStatus InstallProductsHelper(const InstallationState& original_state,
                                    const base::FilePath& setup_exe,
                                    const base::CommandLine& cmd_line,
                                    const MasterPreferences& prefs,
                                    const InstallerState& installer_state,
                                    base::FilePath* installer_directory,
                                    ArchiveType* archive_type,
                                    base::Version* installer_version_returned) {
  DCHECK(archive_type);
  const bool system_install = installer_state.system_install();
  InstallStatus install_status = UNKNOWN_STATUS;

  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForModule(CURRENT_MODULE()));
  if (!file_version_info) {
    VLOG(1) << "Failed to obtain version from resource.";
    return INVALID_ARCHIVE;
  }

  base::string16 installer_version_string =
      file_version_info->product_version();
  // Expected version format is a.b.c.d, so minimum length of 7.
  if (installer_version_string.length() < 7) {
    LOG(ERROR) << "Did not find any valid version in installer.";
    install_status = INVALID_ARCHIVE;
    installer_state.WriteInstallerResult(install_status,
        IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
    return INVALID_ARCHIVE;
  }
  std::unique_ptr<base::Version> installer_version(
      new base::Version(base::UTF16ToASCII(installer_version_string.c_str())));
  if (!installer_version) {
    VLOG(1) << "Failed to allocate version object.";
    return INVALID_ARCHIVE;
  }

  VLOG(1) << "version to install: " << installer_version->GetString();
  if (installer_version_returned) {
    *installer_version_returned = *installer_version;
  }

  bool proceed_with_installation = true;

  if (!IsDowngradeAllowed(prefs)) {
    const ProductState* product_state =
        original_state.GetProductState(system_install);
    if (product_state != NULL &&
        (product_state->version().CompareTo(*installer_version) > 0)) {
      LOG(ERROR) << "Higher version of Edge is already installed.";
      int message_id = IDS_INSTALL_HIGHER_VERSION_BASE;
      proceed_with_installation = false;
      install_status = HIGHER_VERSION_EXISTS;
      installer_state.WriteInstallerResult(install_status, message_id, NULL);
    }
  }

  if (proceed_with_installation) {
    // Create a temp folder where we will unpack Chrome archive. If it fails,
    // then we are doomed, so return immediately and no cleanup is required.
    SelfCleaningTempDir temp_path;
    base::FilePath unpack_path;
    if (!CreateTemporaryAndUnpackDirectories(installer_state, &temp_path,
                                             &unpack_path)) {
      installer_state.WriteInstallerResult(TEMP_DIR_FAILED,
                                           IDS_INSTALL_TEMP_DIR_FAILED_BASE,
                                           NULL);
      return TEMP_DIR_FAILED;
    }

    // Uncompress and optionally patch the archive if an uncompressed archive was
    // not specified on the command line and a compressed archive is found.
    *archive_type = UNKNOWN_ARCHIVE_TYPE;
    base::FilePath uncompressed_archive(cmd_line.GetSwitchValuePath(
        switches::kUncompressedArchive));
    if (uncompressed_archive.empty()) {
      base::Version previous_version;
      if (cmd_line.HasSwitch(installer::switches::kPreviousVersion)) {
        previous_version = base::Version(cmd_line.GetSwitchValueASCII(
            installer::switches::kPreviousVersion));
      }

      std::unique_ptr<ArchivePatchHelper> archive_helper(
          CreateChromeArchiveHelper(
              setup_exe, cmd_line, installer_state, unpack_path,
              (previous_version.IsValid()
                  ? UnPackConsumer::CHROME_ARCHIVE_PATCH
                  : UnPackConsumer::COMPRESSED_CHROME_ARCHIVE)));
      if (archive_helper) {
        VLOG(1) << "Installing Edge from compressed archive "
                << PII_STRING(
                    archive_helper->compressed_archive().value());
        if (!UncompressAndPatchChromeArchive(original_state,
                                            installer_state,
                                            archive_helper.get(),
                                            archive_type,
                                            &install_status,
                                            previous_version)) {
          DCHECK_NE(install_status, UNKNOWN_STATUS);
          return install_status;
        }
        uncompressed_archive = archive_helper->target();
        DCHECK(!uncompressed_archive.empty());
      }
    }

    // Check for an uncompressed archive alongside the current executable if one
    // was not given or generated.
    if (uncompressed_archive.empty())
      uncompressed_archive = setup_exe.DirName().Append(kChromeArchive);

    if (*archive_type == UNKNOWN_ARCHIVE_TYPE) {
      // An archive was not uncompressed or patched above.
      if (uncompressed_archive.empty() ||
          !base::PathExists(uncompressed_archive)) {
        LOG(ERROR) << "Cannot install Edge without an uncompressed archive.";
        installer_state.WriteInstallerResult(
            INVALID_ARCHIVE, IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);

        return INVALID_ARCHIVE;
      }
      *archive_type = FULL_ARCHIVE_TYPE;
    }

    // Unpack the uncompressed archive.
    // UMA tells us the following about the time required to unpack as of M75:
    // --- Foreground ---
    //   <2.7s (50%ile) / 45s (99%ile)
    // --- Background ---
    //   ~14s (50%ile) / >3m (99%ile)
    //
    // The top unpack failure result with 28 days aggregation (>=0.01%)
    // Setup.Install.LzmaUnPackResult_UncompressedChromeArchive
    // 0.66% DISK_FULL
    // 0.04% ACCESS_DENIED
    // 0.01% INVALID_HANDLE
    // 0.01% ERROR_NO_SYSTEM_RESOURCES
    // 0.01% PATH_NOT_FOUND
    // 0.01% ERROR_IO_DEVICE
    //
    // More information can also be found with metric:
    // Setup.Install.LzmaUnPackNTSTATUS_UncompressedChromeArchive
    installer_state.SetStage(UNPACKING);
    UnPackStatus unpack_status = UnPackArchive(uncompressed_archive, unpack_path,
                                               /*output_file=*/nullptr);
    RecordUnPackMetrics(unpack_status,
                        UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE);
    if (unpack_status != UNPACK_NO_ERROR) {
      installer_state.WriteInstallerResult(
          UNPACKING_FAILED,
          IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
          NULL);

      return UNPACKING_FAILED;
    }

    VLOG(1) << "unpacked to " << PII_STRING(unpack_path.value());
    base::FilePath src_path(
        unpack_path.Append(kInstallSourceChromeDir));

    base::FilePath prefs_source_path(cmd_line.GetSwitchValueNative(
        switches::kInstallerData));
    install_status = InstallOrUpdateProduct(
        original_state, installer_state, setup_exe, uncompressed_archive,
        temp_path.path(), src_path, prefs_source_path, prefs,
        *installer_version);

    int install_msg_base = IDS_INSTALL_FAILED_BASE;
    base::FilePath chrome_exe;
    base::string16 quoted_chrome_exe;
    if (install_status == SAME_VERSION_REPAIR_FAILED) {
      install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_BASE;
    } else if (install_status != INSTALL_FAILED) {
      if (installer_state.target_path().empty()) {
        // If we failed to construct install path, it means the OS call to
        // get %ProgramFiles% or %AppData% failed. Report this as failure.
        install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
        install_status = OS_ERROR;
      } else {
        chrome_exe = installer_state.target_path();
        const wchar_t* exe_name;
        if (installer_state.is_webview()) {
          chrome_exe = chrome_exe.AppendASCII(installer_version->GetString());
          exe_name = kWebViewExe;
        } else {
          exe_name = kChromeExe;
        }
        chrome_exe = chrome_exe.Append(exe_name);
        quoted_chrome_exe = L"\"" + chrome_exe.value() + L"\"";
        install_msg_base = 0;
      }
    }

    installer_state.SetStage(FINISHING);

    // MSFT: we are now at the install FINISHING stage
    // if Shell is doing migration,
    // in order to avoid some syncing user exp issues
    // we will wait for taskbar migration finish
    // or timeout
    if (edgeuwp_util::IsMigrating()) {
      edgeuwp_util::WaitForTaskbarMigrationDone();
    }

    bool do_not_register_for_update_launch = false;
    prefs.GetBool(master_preferences::kDoNotRegisterForUpdateLaunch,
                  &do_not_register_for_update_launch);

    bool write_chrome_launch_string =
        (!do_not_register_for_update_launch &&
          install_status != IN_USE_UPDATED);

    installer_state.WriteInstallerResult(install_status, install_msg_base,
        write_chrome_launch_string ? &quoted_chrome_exe : NULL);

    if (install_status == FIRST_INSTALL_SUCCESS) {
      VLOG(1) << "First install successful.";
      if (installer_state.is_browser()) {
        // We never want to launch Chrome in system level install mode.
        bool do_not_launch_chrome = false;
        prefs.GetBool(master_preferences::kDoNotLaunchChrome,
                      &do_not_launch_chrome);

        // The installer can't pin a profile-specific shortcut to the taskbar
        // so we'll have msedge.exe do it. On Windows 10 this will cause a new
        // shortcut to be pinned, or the already migrated Edge Legacy shortcut
        // to be updated. On Windows 7 and 8, the shortcut pinned by the
        // installer will be updated to point to the primary profile.
        if (InstallUtil::IsRunningAsInteractiveUser())
          ShellUtil::SetAutoPinRegistryKey();

        if (!system_install && !do_not_launch_chrome) {
          base::CommandLine empty_cmdline(base::CommandLine::NO_PROGRAM);
          base::LaunchOptions default_launch_options;
          LaunchChromeBrowser(installer_state.target_path(), empty_cmdline,
                              default_launch_options);
        }
      }
    } else if ((install_status == NEW_VERSION_UPDATED) ||
               (install_status == IN_USE_UPDATED)) {
      DCHECK_NE(chrome_exe.value(), base::string16());
      RemoveChromeLegacyRegistryKeys(chrome_exe);
    }

    // temp_path's dtor will take care of deleting or scheduling itself for
    // deletion at reboot when this scope closes.
    VLOG(1) << "Deleting temporary directory "
            << PII_STRING(temp_path.path().value());
  }

  // If the installation completed successfully...
  if (InstallUtil::GetInstallReturnCode(install_status) == 0) {
    // Update the DisplayVersion created by an MSI-based install.
    base::FilePath master_preferences_file(
      installer_state.target_path().AppendASCII(
          installer::kDefaultMasterPrefs));
    std::string install_id;
    if (prefs.GetString(installer::master_preferences::kMsiProductId,
                        &install_id)) {
      // A currently active MSI install will have specified the master-
      // preferences file on the command-line that includes the product-id.
      // We must delay the setting of the DisplayVersion until after the
      // grandparent "msiexec" process has exited.
      base::FilePath new_setup =
          installer_state.GetInstallerDirectory(*installer_version)
          .Append(kSetupExe);
      DelayedOverwriteDisplayVersions(
          new_setup, install_id, *installer_version);
    } else {
      // Only when called by the MSI installer do we need to delay setting
      // the DisplayVersion.  In other runs, such as those done by the auto-
      // update action, we set the value immediately.
      // Get the app's MSI Product-ID from an entry in ClientState.
      base::string16 app_guid = FindMsiProductId(installer_state);
      if (!app_guid.empty()) {
        OverwriteDisplayVersions(
            app_guid, base::UTF8ToUTF16(installer_version->GetString()));

#if defined(MICROSOFT_EDGE_BUILD)
        if (ShellUtil::IsStickyInstall()) {
          DisableMSIUninstalls(app_guid);
        }
#endif  // defined(MICROSOFT_EDGE_BUILD)
      }
    }
    // Return the path to the directory containing the newly installed
    // setup.exe and uncompressed archive if the caller requested it.
    if (installer_directory) {
      *installer_directory =
          installer_state.GetInstallerDirectory(*installer_version);
    }
  }

  return install_status;
}

}  // namespace installer

int SetupMain(HINSTANCE /*instance*/, HINSTANCE /*prev_instance*/,
              wchar_t* /*command_line*/, int /*show_command*/) {
  // Check to see if the CPU is supported before doing anything else. There's
  // very little than can safely be accomplished if the CPU isn't supported
  // since dependent libraries (e.g., base) may use invalid instructions.
  if (!installer::IsProcessorSupported())
    return installer::CPU_NOT_SUPPORTED;

  // Persist histograms so they can be uploaded later. The storage directory is
  // created during installation when the main WorkItemList is evaluated so
  // disable storage directory creation in PersistentHistogramStorage.
  base::PersistentHistogramStorage persistent_histogram_storage(
      installer::kSetupHistogramAllocatorName,
      base::PersistentHistogramStorage::StorageDirManagement::kUseExisting);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, NULL);

  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    // Histogram storage is enabled at the very top of this wWinMain. Disable it
    // when this process is decicated to crashpad as there is no directory in
    // which to write them nor a browser to subsequently upload them.
    persistent_histogram_storage.Disable();
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess(), base::FilePath(),
        switches::kProcessType, switches::kUserDataDir);
  }

  // install_util uses chrome paths.
  chrome::RegisterPathProvider();

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  installer::InitInstallerLogging(prefs);

  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  VLOG(1) << "Command Line: " << PII_STRING(cmd_line.GetCommandLineString());

  InitializeInstallDetails(cmd_line, prefs);

  bool system_install = false;
  prefs.GetBool(installer::master_preferences::kSystemLevel, &system_install);
  VLOG(1) << "system install is " << system_install;

  InstallationState original_state;
  original_state.Initialize();

  InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, original_state);

  installer::SetupWatchdog setup_watchdog(prefs, installer_state,
      base::TimeDelta::FromMinutes(kWatchdogTimeoutMinutes));

  persistent_histogram_storage.set_storage_base_dir(
      installer_state.target_path());

  installer::ConfigureCrashReporting(installer_state);
  installer::SetInitialCrashKeys(installer_state);
  installer::SetCrashKeysFromCommandLine(cmd_line);

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(cmd_line);

#if defined(ARCH_CPU_64_BITS) || defined(NDEBUG)
  // Disable the handle verifier for all but 32-bit debug builds.
  base::win::DisableHandleVerifier();
#endif

  const bool is_uninstall = cmd_line.HasSwitch(installer::switches::kUninstall);

  // Histogram storage is enabled at the very top of this wWinMain. Disable it
  // during uninstall since there's neither a directory in which to write them
  // nor a browser to subsequently upload them.
  if (is_uninstall)
    persistent_histogram_storage.Disable();

  // Check to make sure current system is Win7 or later. If not, log
  // error message and get out.
  if (!InstallUtil::IsOSSupported()) {
    LOG(ERROR) << "Edge only supports Windows 7 or later. Exit code: "
               << installer::OS_NOT_SUPPORTED;
    installer_state.WriteInstallerResult(
        installer::OS_NOT_SUPPORTED, IDS_INSTALL_OS_NOT_SUPPORTED_BASE, NULL);
    return installer::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded()) {
    LOG(ERROR) << "COM initialization failed. Exit code: "
               << installer::OS_ERROR;
    installer_state.WriteInstallerResult(
        installer::OS_ERROR, IDS_INSTALL_OS_ERROR_BASE, NULL);
    installer::UploadToWatson(prefs, base::Version(),
        installer::OS_ERROR, installer::COLLECT_SETUP_LOG,
        L"EdgeInstallerError");
    return installer::OS_ERROR;
  }

  // Make sure system_level is supported if requested. For historical reasons,
  // system-level installs have never been supported for Chrome canary (SxS).
  // This is a brand-specific policy for this particular mode. In general,
  // system-level installation of secondary install modes is fully supported.
  if (!install_static::InstallDetails::Get().supports_system_level() &&
      (system_install ||
       cmd_line.HasSwitch(installer::switches::kSelfDestruct) ||
       cmd_line.HasSwitch(installer::switches::kRemoveChromeRegistration))) {
    LOG(ERROR) << "System level not supported for this install mode."
               << " Exit code: " << installer::SXS_OPTION_NOT_SUPPORTED;
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }
  // Some switches only apply for modes that can be made the user's default
  // browser.
  if (!install_static::SupportsSetAsDefaultBrowser() &&
      (cmd_line.HasSwitch(installer::switches::kMakeChromeDefault) ||
       cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser))) {
    LOG(ERROR) << "Default browser option not supported for this install mode."
               << " Exit code: " << installer::SXS_OPTION_NOT_SUPPORTED;
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }
  // Some switches only apply for modes that support retention experiments.
  if (!install_static::SupportsRetentionExperiments() &&
      cmd_line.HasSwitch(installer::switches::kUserExperiment)) {
    LOG(ERROR) << "User experiments not supported for this install mode."
               << " Exit code: " << installer::SXS_OPTION_NOT_SUPPORTED;
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }

  // Some command line options are no longer supported and must error out.
  if (installer::ContainsUnsupportedSwitch(cmd_line)) {
    LOG(ERROR) << "Command line contains an unsupported switch. Exit code: "
               << installer::UNSUPPORTED_OPTION;
    return installer::UNSUPPORTED_OPTION;
  }

  // Some command line options are not supported for WebView and must error out.
  if (install_static::InstallDetails::Get().is_webview() &&
      !installer::ContainsOnlySupportedWebViewSwitches(cmd_line)) {
    LOG(ERROR) << "Command line contains an unsupported switch for WebView."
               << " Exit code: " << installer::UNSUPPORTED_OPTION;
    return installer::UNSUPPORTED_OPTION;
  }

  // A variety of installer operations require the path to the current
  // executable. Get it once here for use throughout these operations. Note that
  // the path service is the authoritative source for this path. One might think
  // that CommandLine::GetProgram would suffice, but it won't since
  // CreateProcess may have been called with a command line that is somewhat
  // ambiguous (e.g., an unquoted path with spaces, or a path lacking the file
  // extension), in which case CommandLineToArgv will not yield an argv with the
  // true path to the program at position 0.
  base::FilePath setup_exe;
  base::PathService::Get(base::FILE_EXE, &setup_exe);

  // If setup.exe is under the product, the path would look like
  // Microsoft\<Product>\Application\<Version>\Installer\setup.exe, so the
  // full product name is 4 directories up from setup.exe.
  base::FilePath full_product_name_dir =
      setup_exe.DirName().DirName().DirName().DirName();
  // Sanity check if company name is the product's parent directory.
  if (_wcsicmp(full_product_name_dir.DirName().BaseName().value().c_str(),
               install_static::kCompanyPathName) == 0) {
    std::wstring full_product_name = full_product_name_dir.BaseName().value();
    // If the current executable is run under a browser product's install path
    // but we're trying to install/uninstall the WebView, error out.
    if (install_static::IsProductNameFromCategory(
            full_product_name, install_static::ProductCategory::Browser) &&
        install_static::InstallDetails::Get().is_webview()) {
      LOG(ERROR) << "Installing/Uninstalling WebView from browser not"
                 << " supported. Exit code: "
                 << installer::MISMATCHED_PRODUCT_BROWSER;
      installer::UploadToWatson(
          prefs, base::Version(), installer::MISMATCHED_PRODUCT_BROWSER,
          installer::COLLECT_SETUP_LOG, L"EdgeInstallerError");
      return installer::MISMATCHED_PRODUCT_BROWSER;
    }
    // If the current executable is run under a WebView product's install path
    // but we're trying to install/uninstall the browser, error out.
    if (install_static::IsProductNameFromCategory(
            full_product_name, install_static::ProductCategory::WebView) &&
        install_static::InstallDetails::Get().is_browser()) {
      LOG(ERROR) << "Installing/Uninstalling browser from WebView not"
                 << " supported. Exit code: "
                 << installer::MISMATCHED_PRODUCT_WEBVIEW;
      installer::UploadToWatson(
          prefs, base::Version(), installer::MISMATCHED_PRODUCT_WEBVIEW,
          installer::COLLECT_SETUP_LOG, L"EdgeInstallerError");
      return installer::MISMATCHED_PRODUCT_WEBVIEW;
    }
  }

  int exit_code = 0;
  if (HandleNonInstallCmdLineOptions(setup_exe, cmd_line, &original_state,
                                     &installer_state, &exit_code)) {
    VLOG(1) << "NonInstall operation completed. Exit code: " << exit_code;
    installer::InstallStatus install_status =
        static_cast<installer::InstallStatus>(exit_code);
    if (installer::ShouldCollectWatson(install_status)) {
      installer::UploadToWatson(prefs, base::Version(), install_status,
        installer::COLLECT_SETUP_LOG, L"EdgeInstallerError");
    }
    return exit_code;
  }

  if (system_install && !IsUserAnAdmin()) {
    if (!cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      base::CommandLine new_cmd(base::CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer::switches::kRunAsAdmin);
      // If system_install became true due to an environment variable, append
      // it to the command line here since env vars may not propagate past the
      // elevation.
      if (!new_cmd.HasSwitch(installer::switches::kSystemLevel))
        new_cmd.AppendSwitch(installer::switches::kSystemLevel);

      DWORD exit_code = installer::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exit_code);
      return exit_code;
    } else {
      LOG(ERROR) << "Non admin user can not install system level Edge."
                 << " Exit code: " << installer::INSUFFICIENT_RIGHTS;
      installer_state.WriteInstallerResult(installer::INSUFFICIENT_RIGHTS,
          IDS_INSTALL_INSUFFICIENT_RIGHTS_BASE, NULL);
      return installer::INSUFFICIENT_RIGHTS;
    }
  }

  std::unique_ptr<installer::SetupSingleton> setup_singleton(
      installer::SetupSingleton::Acquire(cmd_line, prefs, &original_state,
                                         &installer_state));
  if (!setup_singleton) {
    LOG(ERROR) << "Setup singletion acquisition failed. Exit code: "
               << installer::SETUP_SINGLETON_ACQUISITION_FAILED;
    installer_state.WriteInstallerResult(
        installer::SETUP_SINGLETON_ACQUISITION_FAILED,
        IDS_INSTALL_SINGLETON_ACQUISITION_FAILED_BASE, nullptr);
    installer::UploadToWatson(prefs, base::Version(),
        installer::SETUP_SINGLETON_ACQUISITION_FAILED,
        installer::COLLECT_SETUP_LOG, L"EdgeInstallerError");
    return installer::SETUP_SINGLETON_ACQUISITION_FAILED;
  }

  base::FilePath installer_directory;
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  base::Version current_version;
  // If --uninstall option is given, uninstall the identified product(s)
  if (is_uninstall) {
    // Get version from registry before we do uninstall
    current_version = InstallUtil::GetChromeVersion(system_install);

    install_status =
        UninstallProducts(original_state, installer_state, setup_exe, cmd_line);
    if (install_status != installer::UNINSTALL_SUCCESSFUL)
      LOG(ERROR) << L"Error during uninstall - EdgeUninstallError: "
                 << install_status;

    const bool deregister_etw_provider_fail =
        installer::DeRegisterETWProviderFailed();
    if (deregister_etw_provider_fail) {
      LOG(WARNING) << L"Warning during uninstall - "
                    L"EdgeUninstallDeRegisterEtwProviderError (uploading log)";

      installer::UploadToWatson(prefs, current_version, install_status,
          installer::COLLECT_SETUP_LOG,
          L"EdgeUninstallDeRegisterEtwProviderError");
    }
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    install_status = InstallProducts(original_state, setup_exe, cmd_line, prefs,
                                     &installer_state, &installer_directory,
                                     &current_version);

    DoLegacyCleanups(installer_state, install_status);

    // It may be time to kick off an experiment if this was a successful update
    // and Chrome was not in use (since the experiment only applies to inactive
    // installs).
    if (install_status == installer::NEW_VERSION_UPDATED &&
        installer::ShouldRunUserExperiment(installer_state)) {
      installer::BeginUserExperiment(
          installer_state, installer_directory.Append(setup_exe.BaseName()),
          !system_install);
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Setup.Install.Result", install_status,
                            installer::MAX_INSTALL_STATUS);
  UMA_HISTOGRAM_ENUMERATION("Microsoft.Startup.SetupResult", install_status,
                            installer::MAX_INSTALL_STATUS);

  // Dump peak memory usage.
  PROCESS_MEMORY_COUNTERS pmc;
  if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
    UMA_HISTOGRAM_MEMORY_KB("Setup.Install.PeakPagefileUsage",
                            base::saturated_cast<base::HistogramBase::Sample>(
                                pmc.PeakPagefileUsage / 1024));
    UMA_HISTOGRAM_MEMORY_KB("Setup.Install.PeakWorkingSetSize",
                            base::saturated_cast<base::HistogramBase::Sample>(
                                pmc.PeakWorkingSetSize / 1024));
  }
  auto process_metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();
  auto disk_usage = process_metrics->GetCumulativeDiskUsageInBytes();
  base::UmaHistogramMemoryMB(
      "Setup.Install.CumulativeDiskUsage",
      base::saturated_cast<int>(base::ClampAdd(disk_usage, 1024 * 1024 / 2) /
                                1024 * 1024));

  int return_code = 0;
  // MSI demands that custom actions always return 0 (ERROR_SUCCESS) or it will
  // rollback the action. If we're uninstalling we want to avoid this, so always
  // report success, squashing any more informative return codes.
  if (!(installer_state.is_msi() && is_uninstall)) {
    // Note that we allow the status installer::UNINSTALL_REQUIRES_REBOOT
    // to pass through, since this is only returned on uninstall which is
    // never invoked directly by Google Update.
    return_code = InstallUtil::GetInstallReturnCode(install_status);
  }

  VLOG(1) << "Installation complete, returning: " << return_code;

  if (installer::ShouldCollectWatson(install_status)) {
    installer::UploadToWatson(prefs, current_version, install_status,
        installer::COLLECT_SETUP_LOG, L"EdgeInstallerError");
  }

  return return_code;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  int result;
  if (InstallUtil::IsRunningAsLocalSystem()) {
    base::ScopedDisallowHKCU should_not_use_hkcu;
    result = SetupMain(instance, prev_instance, command_line, show_command);
  } else {
    result = SetupMain(instance, prev_instance, command_line, show_command);
  }
  return result;
}
