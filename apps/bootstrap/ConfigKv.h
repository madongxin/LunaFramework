#pragma once

#include <string>
#include <vector>

/** Read key=value from a flat cnf file (first match). */
std::string LoadCnfValue(const std::string &path, const char *key);

/** Split comma-separated address list; trims spaces. */
std::vector<std::string> SplitAddrList(const std::string &csv);

/** Apply gateway_push_addrs from cnf into GatewayPushClient + StaticServiceRegistry. */
void ApplyGatewayPushAddrsFromConfig();
