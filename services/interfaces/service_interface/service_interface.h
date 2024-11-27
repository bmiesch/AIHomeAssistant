#pragma once

#include <string>
#include <memory>

class IService {
private:
    virtual void Run() = 0;

public:
    virtual ~IService() = default;

    // Core lifecycle methods
    virtual void Initialize() = 0;
    virtual void Stop() = 0;
};