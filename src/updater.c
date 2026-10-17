#include "updater.h"
#include "version.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601
  #endif
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
  #include <winhttp.h>
#else
  #include <unistd.h>
#endif

bool SemVer_Parse(const char *version_str, int *major, int *minor, int *patch)
{
    if (!version_str || !major || !minor || !patch)
        return false;

    *major = 0;
    *minor = 0;
    *patch = 0;

    const char *p = version_str;
    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p == 'v' || *p == 'V')
        p++;

    if (!isdigit((unsigned char)*p))
        return false;

    *major = (int)strtol(p, (char **)&p, 10);
    if (*p == '.') {
        p++;
        if (isdigit((unsigned char)*p)) {
            *minor = (int)strtol(p, (char **)&p, 10);
            if (*p == '.') {
                p++;
                if (isdigit((unsigned char)*p)) {
                    *patch = (int)strtol(p, (char **)&p, 10);
                }
            }
        }
    }

    return true;
}

int SemVer_Compare(const char *v1, const char *v2)
{
    int maj1 = 0, min1 = 0, pat1 = 0;
    int maj2 = 0, min2 = 0, pat2 = 0;

    bool ok1 = SemVer_Parse(v1, &maj1, &min1, &pat1);
    bool ok2 = SemVer_Parse(v2, &maj2, &min2, &pat2);

    if (!ok1 && !ok2) return 0;
    if (!ok1) return -1;
    if (!ok2) return 1;

    if (maj1 != maj2) return (maj1 > maj2) ? 1 : -1;
    if (min1 != min2) return (min1 > min2) ? 1 : -1;
    if (pat1 != pat2) return (pat1 > pat2) ? 1 : -1;

    return 0;
}

bool Updater_ReplaceAndRollback(const char *current_exe, const char *new_exe, const char *old_exe)
{
    if (!current_exe || !new_exe || !old_exe)
        return false;

    /* Verify that new_exe exists and is readable */
    FILE *fp_new = fopen(new_exe, "rb");
    if (!fp_new) {
        return false;
    }
    fclose(fp_new);

    /* Remove any existing old backup file */
#ifdef _WIN32
    SetFileAttributesA(old_exe, FILE_ATTRIBUTE_NORMAL);
#endif
    remove(old_exe);

#ifdef _WIN32
    /* Rename current binary to backup */
    if (!MoveFileExA(current_exe, old_exe, MOVEFILE_REPLACE_EXISTING)) {
        if (rename(current_exe, old_exe) != 0) {
            return false;
        }
    }

    /* Rename new binary to current binary */
    if (!MoveFileExA(new_exe, current_exe, MOVEFILE_REPLACE_EXISTING)) {
        if (rename(new_exe, current_exe) != 0) {
            /* Failed to rename new to current, initiate rollback */
            MoveFileExA(old_exe, current_exe, MOVEFILE_REPLACE_EXISTING);
            return false;
        }
    }
#else
    if (rename(current_exe, old_exe) != 0) {
        return false;
    }
    if (rename(new_exe, current_exe) != 0) {
        rename(old_exe, current_exe);
        return false;
    }
#endif

    return true;
}

bool Updater_DownloadFile(const char *url, const char *dest_path)
{
    if (!url || !dest_path || url[0] == '\0')
        return false;

#ifdef _WIN32
    WCHAR url_w[2048] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, url, -1, url_w, (int)(sizeof(url_w) / sizeof(WCHAR)) - 1) == 0) {
        return false;
    }

    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    WCHAR hostName[256] = {0};
    WCHAR urlPath[2048] = {0};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = (DWORD)(sizeof(hostName) / sizeof(WCHAR));
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = (DWORD)(sizeof(urlPath) / sizeof(WCHAR));

    if (!WinHttpCrackUrl(url_w, (DWORD)wcslen(url_w), 0, &urlComp)) {
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"lords-mobile-bot-updater/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD openFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                                           NULL, NULL,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           openFlags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    /* Enable automatic redirect following across domains (e.g. github.com -> S3) */
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    LPCWSTR headers = L"User-Agent: lords-mobile-bot\r\nAccept: application/octet-stream\r\n";
    BOOL bResults = WinHttpSendRequest(hRequest, headers, (DWORD)-1, NULL, 0, 0, 0);
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (!bResults) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    char chunk[8192];
    DWORD bytesRead = 0;
    bool write_ok = true;
    while (WinHttpReadData(hRequest, chunk, sizeof(chunk), &bytesRead) && bytesRead > 0) {
        if (fwrite(chunk, 1, bytesRead, fp) != bytesRead) {
            write_ok = false;
            break;
        }
    }

    fclose(fp);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!write_ok) {
        remove(dest_path);
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool Updater_CheckGitHubRelease(const char *repo, char *out_version, size_t max_ver_len, char *out_download_url, size_t max_url_len)
{
    if (!repo || !out_version || max_ver_len == 0)
        return false;

    out_version[0] = '\0';
    if (out_download_url && max_url_len > 0)
        out_download_url[0] = '\0';

#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"lords-mobile-bot-updater/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    WCHAR path[256];
    swprintf(path, sizeof(path) / sizeof(WCHAR), L"/repos/%hs/releases/latest", repo);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
                                           NULL, NULL,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    LPCWSTR headers = L"User-Agent: lords-mobile-bot\r\nAccept: application/vnd.github.v3+json\r\n";
    BOOL bResults = WinHttpSendRequest(hRequest, headers, (DWORD)-1,
                                       NULL, 0, 0, 0);

    if (bResults)
        bResults = WinHttpReceiveResponse(hRequest, NULL);

    bool success = false;
    if (bResults) {
        /* Read full response body up to 64KB */
        char buffer[65536];
        size_t total_bytes = 0;
        DWORD bytesRead = 0;
        while (total_bytes + 1 < sizeof(buffer) &&
               WinHttpReadData(hRequest, buffer + total_bytes, (DWORD)(sizeof(buffer) - 1 - total_bytes), &bytesRead) &&
               bytesRead > 0) {
            total_bytes += bytesRead;
        }
        buffer[total_bytes] = '\0';

        if (total_bytes > 0) {
            /* Parse tag_name */
            const char *tag_key = "\"tag_name\":";
            char *tag_pos = strstr(buffer, tag_key);
            if (tag_pos) {
                tag_pos += strlen(tag_key);
                while (*tag_pos && (*tag_pos == ' ' || *tag_pos == '\"')) tag_pos++;
                size_t v_idx = 0;
                while (*tag_pos && *tag_pos != '\"' && *tag_pos != ',' && v_idx + 1 < max_ver_len) {
                    out_version[v_idx++] = *tag_pos++;
                }
                out_version[v_idx] = '\0';
                success = (v_idx > 0);
            }

            /* Parse browser_download_url prioritizing .exe assets */
            if (out_download_url && max_url_len > 0) {
                const char *url_key = "\"browser_download_url\":";
                const char *search_pos = buffer;
                char best_url[512] = {0};
                while ((search_pos = strstr(search_pos, url_key)) != NULL) {
                    search_pos += strlen(url_key);
                    while (*search_pos && (*search_pos == ' ' || *search_pos == '\"')) search_pos++;
                    char temp_url[512] = {0};
                    size_t u_idx = 0;
                    while (*search_pos && *search_pos != '\"' && *search_pos != ',' && u_idx + 1 < sizeof(temp_url)) {
                        temp_url[u_idx++] = *search_pos++;
                    }
                    temp_url[u_idx] = '\0';
                    if (best_url[0] == '\0') {
                        strncpy(best_url, temp_url, sizeof(best_url) - 1);
                    }
                    if (strstr(temp_url, ".exe") != NULL) {
                        strncpy(best_url, temp_url, sizeof(best_url) - 1);
                        break;
                    }
                }
                strncpy(out_download_url, best_url, max_url_len - 1);
                out_download_url[max_url_len - 1] = '\0';
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
#else
    return false;
#endif
}

bool Updater_ExecuteUpdate(Connection *c, char *out_message, size_t max_msg_len)
{
    if (!out_message || max_msg_len == 0)
        return false;

    char current_exe[512] = {0};
    char new_exe[520] = {0};
    char old_exe[520] = {0};

#ifdef _WIN32
    if (GetModuleFileNameA(NULL, current_exe, sizeof(current_exe) - 1) == 0) {
        snprintf(current_exe, sizeof(current_exe), "lords-mobile-bot.exe");
    }
#else
    snprintf(current_exe, sizeof(current_exe), "lords-mobile-bot");
#endif

    snprintf(new_exe, sizeof(new_exe), "%s.new", current_exe);
    snprintf(old_exe, sizeof(old_exe), "%s.old", current_exe);

    /* 1. If lords-mobile-bot.exe.new already exists locally, trigger immediate replacement */
    FILE *fp_test = fopen(new_exe, "rb");
    if (fp_test) {
        fclose(fp_test);
        if (Updater_ReplaceAndRollback(current_exe, new_exe, old_exe)) {
            snprintf(out_message, max_msg_len,
                     "Binary replaced successfully (%s -> %s). Launching new version...",
                     new_exe, current_exe);

#ifdef _WIN32
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            char cmdline[1024];
            const char *cfg = (c && c->config_path[0]) ? c->config_path : "config.cfg";
            snprintf(cmdline, sizeof(cmdline), "\"%s\" \"%s\"", current_exe, cfg);

            if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                if (c) c->running = false;
                return true;
            }
#endif
            if (c) c->running = false;
            return true;
        } else {
            snprintf(out_message, max_msg_len,
                     "Failed to replace binary with %s. Rollback preserved original binary.",
                     new_exe);
            return false;
        }
    }

    /* 2. Query GitHub Releases for latest release */
    char latest_ver[64] = {0};
    char download_url[512] = {0};
    if (!Updater_CheckGitHubRelease("halloweeks/lords-mobile-bot", latest_ver, sizeof(latest_ver), download_url, sizeof(download_url))) {
        snprintf(out_message, max_msg_len,
                 "No local .new binary found and unable to reach GitHub Releases API.");
        return false;
    }

    if (SemVer_Compare(latest_ver, VERSION) <= 0) {
        snprintf(out_message, max_msg_len,
                 "Already up to date (Current: v%s, Latest: v%s).", VERSION, latest_ver);
        return true;
    }

    /* 3. New version found: download release binary to .new */
    if (download_url[0] == '\0') {
        snprintf(out_message, max_msg_len,
                 "New version v%s detected (Current: v%s), but no download asset URL found.",
                 latest_ver, VERSION);
        return false;
    }

    LOGI("[UPDATER] Downloading update v%s from %s to %s...\n", latest_ver, download_url, new_exe);
    if (!Updater_DownloadFile(download_url, new_exe)) {
        snprintf(out_message, max_msg_len,
                 "Found new version v%s, but failed to download update binary from GitHub.",
                 latest_ver);
        return false;
    }

    /* 4. Binary downloaded: perform in-place replacement and restart */
    if (Updater_ReplaceAndRollback(current_exe, new_exe, old_exe)) {
        snprintf(out_message, max_msg_len,
                 "Update v%s installed successfully. Launching new version...",
                 latest_ver);

#ifdef _WIN32
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        char cmdline[1024];
        const char *cfg = (c && c->config_path[0]) ? c->config_path : "config.cfg";
        snprintf(cmdline, sizeof(cmdline), "\"%s\" \"%s\"", current_exe, cfg);

        if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (c) c->running = false;
            return true;
        }
#endif
        if (c) c->running = false;
        return true;
    } else {
        snprintf(out_message, max_msg_len,
                 "Failed to replace binary with downloaded v%s. Rollback preserved original binary.",
                 latest_ver);
        return false;
    }
}
