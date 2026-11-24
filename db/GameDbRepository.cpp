/**
 * GameDbRepository.cpp — MySQL/GameDB：游戏逻辑。数据库。
 * 路径: db/GameDbRepository.cpp
 */
#include "GameDbRepository.h"

namespace {
IGameDbRepository *g_repo = nullptr;
}

void GameDbRepository::Set(IGameDbRepository *repo) { g_repo = repo; }

IGameDbRepository *GameDbRepository::Get() { return g_repo; }
