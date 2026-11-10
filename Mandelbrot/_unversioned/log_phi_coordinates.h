#pragma once
#include <bitloop.h>

SIM_BEG;
using namespace bl;

struct Cx { 
    double re, im; 
};

class LogPhiCoordinates : public ChangeTracker
{
    f64 input_angle = 0.0;
    int input_iters = 20;
    int input_quality = 2;

    f64 plot_x, plot_y;
    std::vector<std::vector<DVec2>> iter_paths;
    std::vector<std::vector<DVec2>> ring_paths;
    void rebuildRingPaths();

public:

    void init();
    void process();
    void draw(Viewport* ctx) const;
};

SIM_END;