#pragma once

#include <string>

class EventLoop;

void BootstrapInstallPeriodicTasks(EventLoop *loop, const std::string &role);
