#pragma once

#include "DartApp.h"

class PuredartWriter {
public:
    explicit PuredartWriter(DartApp& app) : app(app) {}
    void Create(const char* filename);

private:
    DartApp& app;
};
