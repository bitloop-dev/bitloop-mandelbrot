#pragma once
#include <bitloop.h>

SIM_BEG;
using namespace bl;

using flt = fdd;

struct BabyMandelEntry
{
    Complex<flt> center;
    Complex<flt> root;
    flt zoom;
    double rotation_rad;
    int period;
    flt back_radius;
    flt scale_radius;
};

struct BabyMandelParams
{
    // _ChatGPT_ region to search in the parameter plane
    double min_re = -2.0;
    double max_re = 1.0;
    double min_im = -1.5;
    double max_im = 1.5;

    // _ChatGPT_ include copies whose cusp scale >= 0.25 / max_zoom
    flt max_zoom = flt(1e5);

    // _ChatGPT_ recursion leaf size = leaf_factor * (0.25 / max_zoom)
    double leaf_factor = 0.50;

    // _ChatGPT_ per-leaf seed sampling grid
    int leaf_samples_axis = 3;

    // _ChatGPT_ period detection for seed classification
    int max_period = 128;
    int detect_iter_max = 1800;
    int detect_warmup = 64;

    flt escape_r2 = flt(4.0);
    flt detect_eps2 = flt(1e-28);

    // _ChatGPT_ center Newton solve f_c^p(0)=0
    int center_newton_max_iters = 80;
    flt center_newton_tol2 = flt(1e-56);
    flt center_newton_min_deriv2 = flt(1e-90);

    // _ChatGPT_ quadtree prune using escape distance estimate
    int prune_iter_max = 250;
    double prune_margin = 1.05;

    // _ChatGPT_ early stop inside a single attracting component when |mult| <= mult_strong
    double mult_strong = 0.60;

    // _ChatGPT_ cusp estimation by radial boundary search
    int cusp_dirs = 24;
    int cusp_binary_steps = 60;
    int cusp_classify_iter_max = 1600;
    flt cusp_classify_eps2 = flt(1e-26);
    flt cusp_r_start = flt(1e-12);
    flt cusp_r_max = flt(2.0);

    // _ChatGPT_ cardioid filter rejects satellite bulbs
    double cardioid_ratio_min = 1.35;
    double cusp_unique_min = 1.03;

    // _ChatGPT_ dedup quantization relative to cusp scale threshold
    double dedup_factor = 0.20;

    // _ChatGPT_ safety cap
    int max_entries = 200000;

    // _ChatGPT_ include the main cardioid as a trivial result
    bool include_main = false;
};

struct QuadCell
{
    double re0 = 0;
    double im0 = 0;
    double w = 0;
    double h = 0;
};

SIM_END;