/**
 * GM 会话 Cookie、简易文件列表/下载、危险调试命令与崩溃触发器。
 * 路由层须 LUNASERVER_ENABLE_ADMIN=1；勿对公网开放。
 */
#include "bootstrap/GmAdmin.h"

#include "bootstrap/AdminHttpUtil.h"

#include "FormalMode.h"
#include "LunaServerPaths.h"
#include "ProcSelfStats.h"
#include "ResourceWatchdog.h"
#include "SafePath.h"

#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

std::mutex g_gm_sess_mu;
std::unordered_set<std::string> g_gm_sessions;


// ----- GM 登录态（进程内 set，重启失效）-----
std::string NewGmSessionId() {
    static const char hex[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    for (int t = 0; t < 100; ++t) {
        std::string s(32, '0');
        for (char &c : s)
            c = hex[gen() & 0xf];
        std::lock_guard<std::mutex> lk(g_gm_sess_mu);
        if (!g_gm_sessions.count(s)) {
            g_gm_sessions.insert(s);
            return s;
        }
    }
    return "gm_fallback";
}

bool GmSessionValid(const std::string &sid) {
    if (sid.empty())
        return false;
    std::lock_guard<std::mutex> lk(g_gm_sess_mu);
    return g_gm_sessions.count(sid) > 0;
}

std::string GmSidFromRequest(const HttpRequest &request) {
    const std::string cookie = request.GetHeader("Cookie");
    const std::string key = "gm_sid=";
    const auto p = cookie.find(key);
    if (p == std::string::npos)
        return "";
    const auto end = cookie.find(';', p);
    return cookie.substr(p + key.size(), end == std::string::npos ? std::string::npos : end - p - key.size());
}


// ----- files/ 目录列表与安全下载（SafeJoinUnderRoot 防路径穿越）-----
void FindAllFiles(const std::string &dir, std::vector<std::string> *out) {
    DIR *d = ::opendir(dir.c_str());
    if (!d)
        return;
    while (dirent *ent = ::readdir(d)) {
        if (ent->d_name[0] == '.')
            continue;
        out->push_back(ent->d_name);
    }
    ::closedir(d);
}

Json::Value BuildFilesJson() {
    std::vector<std::string> names;
    FindAllFiles(LunaServerPaths::FilesRoot(), &names);
    Json::Value arr(Json::arrayValue);
    for (const auto &n : names)
        arr.append(n);
    return arr;
}

std::string BuildFileHtml() {
    std::ostringstream os;
    os << "<html><body><h3>Files</h3><ul>";
    std::vector<std::string> names;
    FindAllFiles(LunaServerPaths::FilesRoot(), &names);
    for (const auto &n : names)
        os << "<li><a href=\"/download?file=" << n << "\">" << n << "</a></li>";
    os << "</ul></body></html>";
    return os.str();
}

bool DownloadFile(const std::string &name, HttpResponse *response, bool head_only) {
    std::string path;
    if (!SafeJoinUnderRoot(LunaServerPaths::FilesRoot(), name, &path)) {
        SendPlain(response, "bad path", HttpResponse::HttpStatusCode::k400BadRequest);
        return false;
    }
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        SendPlain(response, "file not found", HttpResponse::HttpStatusCode::k404NotFound);
        return false;
    }
    struct stat st {};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        return false;
    }
    response->SetStatusCode(HttpResponse::HttpStatusCode::k200K);
    response->SetStatusMessage("OK");
    response->SetContentType("application/octet-stream");
    response->SetContentLength(static_cast<int>(st.st_size));
    response->SetBodyType(HttpResponse::HttpBodyType::FILE_TYPE);
    response->SetFileFd(fd);
    if (head_only)
        ::close(fd);
    return true;
}


// ----- GM 控制台页与命令执行 -----
std::string GmConsolePage(const HttpRequest &, const std::string &sid) {
    std::ostringstream os;
    os << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>GM</title></head><body>"
       << "<p>gm_sid=" << sid << "</p>"
       << "<form method=\"post\" action=\"/gm\"><input name=\"cmd\" size=\"48\" "
          "placeholder=\"help / stat / watch / hcpu / hcpu_stop\"/> "
       << "<button type=\"submit\">run</button></form>"
       << "<p><a href=\"/\">home</a> | <a href=\"/monitor\">monitor</a></p></body></html>";
    return os.str();
}

Json::Value GmExecute(const std::string &cmd) {
    Json::Value root;
    if (cmd == "help") {
        root["help"] = "stat watch history proc_line hcpu hcpu_stop";
        return root;
    }
    if (cmd == "stat" || cmd == "watch") {
        root["last"] = ResourceWatchdog::Instance().LastSampleJson();
        root["trends"] = ResourceWatchdog::Instance().AnalyzeTrends();
        return root;
    }
    if (cmd == "history") {
        root["history"] = ResourceWatchdog::Instance().HistoryJson(120);
        return root;
    }
    if (cmd == "proc_line") {
        ProcResourceSnapshot s;
        FillProcResourceSnapshot(&s);
        root["line"] = ProcResourceSnapshotFormatLine(s);
        return root;
    }
    root["error"] = "unknown cmd";
    return root;
}


// ----- 仅调试：故意崩溃（需 ENABLE_ASAN_CRASH 等显式开关）-----
static int *g_uaf_ptr = nullptr;

void crash_uaf() {
    delete g_uaf_ptr;
    g_uaf_ptr = new int(42);
    *g_uaf_ptr = 1;
    delete g_uaf_ptr;
    g_uaf_ptr = nullptr;
}

void crash_stack_overflow(int depth);

void trigger_stack_overflow() {
    crash_stack_overflow(0);
}

void crash_stack_overflow(int depth) {
    char buf[1024 * 64];
    memset(buf, depth, sizeof(buf));
    crash_stack_overflow(depth + 1);
}

