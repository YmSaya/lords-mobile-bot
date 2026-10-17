#ifndef UPDATER_H
#define UPDATER_H

#include "connection.h"
#include <stdbool.h>
#include <stddef.h>

/* Compare two SemVer strings (e.g. "1.0.3" vs "1.0.2", "v1.10.0" vs "v1.2.0")
 * Returns > 0 if v1 > v2, < 0 if v1 < v2, 0 if v1 == v2.
 */
int SemVer_Compare(const char *v1, const char *v2);

/* Parse SemVer string into major, minor, patch */
bool SemVer_Parse(const char *version_str, int *major, int *minor, int *patch);

/* In-place replace current binary with new binary, backing up current to old.
 * Provides rollback protection: if replacing new fails, restores old.
 */
bool Updater_ReplaceAndRollback(const char *current_exe, const char *new_exe, const char *old_exe);

/* Check GitHub Releases for newer version */
bool Updater_CheckGitHubRelease(const char *repo, char *out_version, size_t max_ver_len, char *out_download_url, size_t max_url_len);

/* Download file from URL to destination path */
bool Updater_DownloadFile(const char *url, const char *dest_path);

/* Execute full update check, download, and in-place replacement */
bool Updater_ExecuteUpdate(Connection *c, char *out_message, size_t max_msg_len);

#endif /* UPDATER_H */
