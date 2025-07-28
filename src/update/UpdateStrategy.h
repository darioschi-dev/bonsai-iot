#pragma once

class UpdateStrategy {
public:
    virtual bool checkForUpdate() = 0;
    virtual bool performUpdate() = 0;
    virtual const char* getName() = 0;
    virtual ~UpdateStrategy() {}
};
