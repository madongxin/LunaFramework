#pragma once


/**
 * GameDbRepository.h — GameDB 仓储接口的本地 MySQL 实现。
 * 路径: db/GameDbRepository.h
 */
#include "IGameDbRepository.h"

/** 进程内 GameDB 后端指针（类似 GameRequestTransport） */
class GameDbRepository {
public:
    static void Set(IGameDbRepository *repo);
    static IGameDbRepository *Get();
};
