#pragma once
#include <bitloop.h>
#include "log_phi_coordinates.h"
#include "minibrot_atlas.h"

SIM_BEG;

using namespace bl;

class Mandel_Experimental : public ChangeTracker
{
    const bool enabled_logphi = true;
    const bool enabled_minibrotAtlas = false;

    // --- alternative mandelbrot coordinate system ---
    LogPhiCoordinates logphi_coords;

    // --- minibrot atlas ---
    BabyMandelParams params;

public:

    void init();
    void process();
    void draw(Viewport* ctx) const;
};

SIM_END;