/**
 * @file GameDbRepository.cpp
 * @brief GameDbRepository::Set/Get 静态指针
 */

#include "GameDbRepository.h"

namespace {
IGameDbRepository *g_repo = nullptr;
}

void GameDbRepository::Set(IGameDbRepository *repo) { g_repo = repo; }

IGameDbRepository *GameDbRepository::Get() { return g_repo; }
