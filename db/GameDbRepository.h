#pragma once

/**
 * @file GameDbRepository.h
 * @brief 进程内 IGameDbRepository* 注册表（Set/Get）
 *
 * GameDB 进程挂 AsyncMysql；Gateway/Logic 挂 BrpcGameDbRepository。
 */

#include "IGameDbRepository.h"

class GameDbRepository {
public:
    /** 注册本进程 GameDB 后端（不拥有所有权） */
    static void Set(IGameDbRepository *repo);
    /** 未 Set 时返回 nullptr */
    static IGameDbRepository *Get();
};
