#pragma once

#include <string>

/** Process-local service registrations for graceful DRAINING / Unregister / Renew. */
namespace LocalRegistry {

void Track(const std::string &service, const std::string &instance_id, int ttl_sec = 30);
void MarkDraining();
void UnregisterAll();
void RenewAll();

std::string ResolveInstanceId(const std::string &prefix, int port, const char *fallback);

}  // namespace LocalRegistry
