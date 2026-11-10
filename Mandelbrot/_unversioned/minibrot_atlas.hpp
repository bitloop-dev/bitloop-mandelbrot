#pragma once
#include "minibrot_atlas.h"

#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <deque>


SIM_BEG;
using namespace bl;

using flt = fdd;

template<typename T>
static constexpr Complex<T> bl_complex_add_real(const Complex<T>& z, T r) { return Complex<T>(z.x + r, z.y); }

template<typename T>
static constexpr Complex<T> bl_complex_mul_scalar(const Complex<T>& z, T s) { return Complex<T>(z.x * s, z.y * s); }

template<typename T>
static constexpr T bl_complex_mag2(const Complex<T>& z) { return z.x * z.x + z.y * z.y; }

template<typename T>
static constexpr T bl_complex_dist2(const Complex<T>& a, const Complex<T>& b)
{
    const T dx = a.x - b.x;
    const T dy = a.y - b.y;
    return dx * dx + dy * dy;
}



// _ChatGPT_ fast analytic interior tests to stop refining huge interior regions
static inline bool bl_inside_main_cardioid(double x, double y)
{
    const double x0 = x - 0.25;
    const double q = x0 * x0 + y * y;
    return q * (q + x0) <= 0.25 * y * y;
}

static inline bool bl_inside_period2_bulb(double x, double y)
{
    const double dx = x + 1.0;
    return (dx * dx + y * y) <= (0.25 * 0.25);
}

static inline bool bl_cell_all_inside_fast(const QuadCell& cell, bool& out_all_cardioid, bool& out_all_bulb2)
{
    out_all_cardioid = true;
    out_all_bulb2 = true;

    const double xs[3] = { cell.re0, cell.re0 + 0.5 * cell.w, cell.re0 + cell.w };
    const double ys[3] = { cell.im0, cell.im0 + 0.5 * cell.h, cell.im0 + cell.h };

    for (int iy = 0; iy < 3; ++iy)
    {
        for (int ix = 0; ix < 3; ++ix)
        {
            const double x = xs[ix];
            const double y = ys[iy];

            if (out_all_cardioid && !bl_inside_main_cardioid(x, y))
                out_all_cardioid = false;

            if (out_all_bulb2 && !bl_inside_period2_bulb(x, y))
                out_all_bulb2 = false;

            if (!out_all_cardioid && !out_all_bulb2)
                return false;
        }
    }

    return out_all_cardioid || out_all_bulb2;
}

static int bl_detect_attracting_period(
    const Complex<flt>& c,
    int max_period,
    int iter_max,
    int warmup,
    flt escape_r2,
    flt eps2)
{
    const int ring_size = std::max(8, max_period * 3);
    std::vector<Complex<flt>> ring((size_t)ring_size, Complex<flt>((flt)0, (flt)0));

    Complex<flt> z((flt)0, (flt)0);

    for (int n = 0; n < iter_max; ++n)
    {
        z = z.squared() + c;
        ring[(size_t)(n % ring_size)] = z;

        if (z.mag2() > escape_r2)
            return 0;

        if (n < warmup)
            continue;

        for (int p = 1; p <= max_period; ++p)
        {
            if (n < 2 * p)
                continue;

            const Complex<flt>& z1 = ring[(size_t)((n - p) % ring_size)];
            const Complex<flt>& z2 = ring[(size_t)((n - 2 * p) % ring_size)];

            if ((z - z1).mag2() < eps2 && (z - z2).mag2() < eps2 * (flt)4)
                return p;
        }
    }

    return 0;
}

static bool bl_estimate_cycle_multiplier2(
    const Complex<flt>& c,
    int period,
    int settle_iters,
    flt escape_r2,
    flt& out_mult2)
{
    Complex<flt> z((flt)0, (flt)0);

    for (int i = 0; i < settle_iters; ++i)
    {
        z = z.squared() + c;
        if (z.mag2() > escape_r2)
            return false;
    }

    // _ChatGPT_ approximate multiplier by product of f'(z)=2z along one period on the settled cycle
    Complex<flt> m((flt)1, (flt)0);
    for (int i = 0; i < period; ++i)
    {
        m *= (z * (flt)2);
        z = z.squared() + c;
        if (z.mag2() > escape_r2)
            return false;
    }

    out_mult2 = m.mag2();
    return true;
}

static bool bl_compute_center_newton(Complex<flt> c, int period, const BabyMandelParams& p, Complex<flt>& out_center)
{
    for (int it = 0; it < p.center_newton_max_iters; ++it)
    {
        Complex<flt> z((flt)0, (flt)0);
        Complex<flt> dzdc((flt)0, (flt)0);

        for (int i = 0; i < period; ++i)
        {
            // _ChatGPT_ dzdc_{n+1} = 2 z_n dzdc_n + 1
            dzdc = bl_complex_add_real(bl_complex_mul_scalar(z * dzdc, (flt)2), (flt)1);
            z = z.squared() + c;
        }

        if (z.mag2() < p.center_newton_tol2)
        {
            out_center = c;
            return true;
        }

        if (dzdc.mag2() < p.center_newton_min_deriv2)
            return false;

        c = c - (z / dzdc);
    }

    return false;
}

static int bl_minimal_period_at_center(const Complex<flt>& c, int p, flt tol2)
{
    for (int d = 1; d < p; ++d)
    {
        if ((p % d) != 0)
            continue;

        Complex<flt> z((flt)0, (flt)0);
        for (int i = 0; i < d; ++i)
            z = z.squared() + c;

        if (z.mag2() < tol2)
            return d;
    }

    return p;
}

static bool bl_escape_distance_estimate(
    const Complex<flt>& c,
    int iter_max,
    flt escape_r2,
    double& out_dist)
{
    Complex<flt> z((flt)0, (flt)0);
    Complex<flt> dzdc((flt)0, (flt)0);

    for (int i = 0; i < iter_max; ++i)
    {
        dzdc = Complex<flt>(dzdc.x * (flt)2 * z.x - dzdc.y * (flt)2 * z.y + (flt)1,
            dzdc.x * (flt)2 * z.y + dzdc.y * (flt)2 * z.x);
        z = z.squared() + c;

        if (z.mag2() > escape_r2)
        {
            const double zx = (double)z.x;
            const double zy = (double)z.y;
            const double dx = (double)dzdc.x;
            const double dy = (double)dzdc.y;

            const double modz = std::sqrt(zx * zx + zy * zy);
            const double moddz = std::sqrt(dx * dx + dy * dy);

            if (modz > 0.0 && moddz > 0.0)
            {
                out_dist = (modz * std::log(modz)) / moddz;
                return true;
            }

            out_dist = 0.0;
            return true;
        }
    }

    return false;
}

static bool bl_in_component_loose(const Complex<flt>& c, int target_period, const BabyMandelParams& p)
{
    const int per = bl_detect_attracting_period(
        c,
        p.max_period,
        p.cusp_classify_iter_max,
        p.detect_warmup,
        p.escape_r2,
        p.cusp_classify_eps2);

    return per == target_period;
}

static bool bl_estimate_cardioid_cusp(
    const Complex<flt>& center,
    int period,
    const BabyMandelParams& p,
    Complex<flt>& out_cusp,
    flt& out_rmin,
    flt& out_rmax,
    double& out_ratio,
    double& out_cusp_unique)
{
    const int dirs = std::max(8, p.cusp_dirs);
    const double two_pi = 6.283185307179586476925286766559;

    flt rmin = p.cusp_r_max;
    flt r2min = p.cusp_r_max;
    flt rmax = (flt)0;

    int imin = -1;
    int imax = -1;

    for (int k = 0; k < dirs; ++k)
    {
        const double ang = two_pi * (double)k / (double)dirs;
        const flt dx = (flt)std::cos(ang);
        const flt dy = (flt)std::sin(ang);
        const Complex<flt> dir(dx, dy);

        flt r_in = (flt)0;
        flt r_out = p.cusp_r_start;

        auto inside = [&](flt r) -> bool
        {
            const Complex<flt> c = center + bl_complex_mul_scalar(dir, r);
            return bl_in_component_loose(c, period, p);
        };

        // _ChatGPT_ find an outside bracket
        while (r_out < p.cusp_r_max && inside(r_out))
        {
            r_in = r_out;
            r_out = r_out * (flt)2;
        }

        if (r_out >= p.cusp_r_max)
            continue;

        if (!inside(r_in))
            continue;

        for (int s = 0; s < p.cusp_binary_steps; ++s)
        {
            const flt mid = (r_in + r_out) * (flt)0.5;
            if (inside(mid))
                r_in = mid;
            else
                r_out = mid;
        }

        const flt r = r_in;

        if (r > rmax)
        {
            rmax = r;
            imax = k;
        }

        if (r < rmin)
        {
            r2min = rmin;
            rmin = r;
            imin = k;
        }
        else if (r < r2min)
        {
            r2min = r;
        }
    }

    if (imin < 0 || imax < 0)
        return false;

    if (rmin <= (flt)0 || rmax <= (flt)0)
        return false;

    out_rmin = rmin;
    out_rmax = rmax;

    out_ratio = (double)(rmax / rmin);
    out_cusp_unique = (double)(r2min / rmin);

    const double ang_min = two_pi * (double)imin / (double)dirs;
    const Complex<flt> dir_min((flt)std::cos(ang_min), (flt)std::sin(ang_min));
    out_cusp = center + bl_complex_mul_scalar(dir_min, rmin);

    return true;
}

struct CenterKey
{
    int64_t qx = 0;
    int64_t qy = 0;
    int period = 0;

    bool operator==(const CenterKey& o) const { return qx == o.qx && qy == o.qy && period == o.period; }
};

struct CenterKeyHash
{
    size_t operator()(const CenterKey& k) const
    {
        uint64_t h = (uint64_t)k.qx * 0x9E3779B185EBCA87ull;
        h ^= (uint64_t)k.qy + 0xC2B2AE3D27D4EB4Full + (h << 6) + (h >> 2);
        h ^= (uint64_t)(uint32_t)k.period + 0x165667B19E3779F9ull + (h << 6) + (h >> 2);
        return (size_t)h;
    }
};

static CenterKey bl_make_center_key(const Complex<flt>& c, int period, double cell)
{
    CenterKey k;
    k.qx = (int64_t)std::llround((double)c.x / cell);
    k.qy = (int64_t)std::llround((double)c.y / cell);
    k.period = period;
    return k;
}

struct BabyMandelAtlasState
{
    std::deque<QuadCell> queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

    std::unordered_set<CenterKey, CenterKeyHash> seen;
    std::mutex seen_mutex;

    std::vector<BabyMandelEntry> out;
    std::mutex out_mutex;

    std::mutex print_mutex;

    std::atomic<int64_t> pending = 0;
    std::atomic<bool> done = false;
};

static void bl_try_seed_cell(
    const QuadCell& cell,
    const BabyMandelParams& p,
    const flt cusp_scale_min,
    double dedup_cell,
    BabyMandelAtlasState& st)
{
    const int sa = std::max(1, p.leaf_samples_axis);

    for (int sy = 0; sy < sa; ++sy)
    {
        for (int sx = 0; sx < sa; ++sx)
        {
            const double fx = ((double)sx + 0.5) / (double)sa;
            const double fy = ((double)sy + 0.5) / (double)sa;

            const double re = cell.re0 + fx * cell.w;
            const double im = cell.im0 + fy * cell.h;

            const Complex<flt> seed((flt)re, (flt)im);

            const int per = bl_detect_attracting_period(seed, p.max_period, p.detect_iter_max, p.detect_warmup, p.escape_r2, p.detect_eps2);
            if (per <= 0)
                continue;

            if (!p.include_main && per == 1)
                continue;

            Complex<flt> center;
            if (!bl_compute_center_newton(seed, per, p, center))
                continue;

            const int per_min = bl_minimal_period_at_center(center, per, p.center_newton_tol2);
            if (per_min != per)
                continue;

            // _ChatGPT_ cheap lower bound: if the component boundary is closer than cusp_scale_min along any axis, skip
            {
                const flt r = cusp_scale_min;
                const Complex<flt> ax[4] = {
                    Complex<flt>( r, (flt)0),
                    Complex<flt>(-r, (flt)0),
                    Complex<flt>((flt)0,  r),
                    Complex<flt>((flt)0, -r)
                };

                bool ok = true;
                for (int k = 0; k < 4; ++k)
                {
                    if (!bl_in_component_loose(center + ax[k], per, p))
                    {
                        ok = false;
                        break;
                    }
                }

                if (!ok)
                    continue;
            }

            Complex<flt> cusp;
            flt rmin = (flt)0;
            flt rmax = (flt)0;
            double ratio = 0.0;
            double cusp_unique = 0.0;

            if (!bl_estimate_cardioid_cusp(center, per, p, cusp, rmin, rmax, ratio, cusp_unique))
                continue;

            if (rmin < cusp_scale_min)
                continue;

            if (ratio < p.cardioid_ratio_min)
                continue;

            if (cusp_unique < p.cusp_unique_min)
                continue;

            // _ChatGPT_ dedup at acceptance to avoid dropping a center due to a transient cusp estimation failure
            {
                const CenterKey key = bl_make_center_key(center, per, dedup_cell);
                std::lock_guard<std::mutex> lock(st.seen_mutex);
                if (st.seen.find(key) != st.seen.end())
                    continue;
                st.seen.insert(key);
            }

            BabyMandelEntry e;
            e.center = center;
            e.root = cusp;
            e.scale_radius = rmin;
            e.back_radius = rmax;
            e.zoom = flt(0.25) / rmin;

            const Complex<double> d = (Complex<double>)(e.root - e.center);
            e.rotation_rad = -std::atan2(d.y, d.x);
            e.period = per;

            {
                std::lock_guard<std::mutex> lock(st.out_mutex);
                if ((int)st.out.size() >= p.max_entries)
                    return;
                st.out.push_back(e);
            }

            {
                std::lock_guard<std::mutex> lock(st.print_mutex);
                blPrint() << "Accepted minibrot: center=" << e.center
                          << " root=" << e.root
                          << " period=" << e.period
                          << " zoom=" << e.zoom
                          << " ratio=" << ratio
                          << "\n";
            }
        }
    }
}

static void bl_process_cell(
    const QuadCell& cell,
    const BabyMandelParams& p,
    const double leaf_target,
    const flt cusp_scale_min,
    const double dedup_cell,
    BabyMandelAtlasState& st)
{
    const double max_dim = std::max(cell.w, cell.h);

    if (max_dim <= leaf_target)
    {
        bl_try_seed_cell(cell, p, cusp_scale_min, dedup_cell, st);
        return;
    }

    // _ChatGPT_ cheap interior stop for main cardioid and period-2 bulb
    bool all_cardioid = false;
    bool all_bulb2 = false;
    if (bl_cell_all_inside_fast(cell, all_cardioid, all_bulb2))
    {
        if (all_cardioid)
        {
            if (p.include_main)
                bl_try_seed_cell(cell, p, cusp_scale_min, dedup_cell, st);
            return;
        }

        // _ChatGPT_ period-2 bulb interior cannot contain other components
        if (all_bulb2)
            return;
    }

    // _ChatGPT_ outside prune by escape distance estimate on corners + center
    const double half_w = cell.w * 0.5;
    const double half_h = cell.h * 0.5;

    const double sample[5][2] =
    {
        { cell.re0,             cell.im0             },
        { cell.re0 + cell.w,    cell.im0             },
        { cell.re0,             cell.im0 + cell.h    },
        { cell.re0 + cell.w,    cell.im0 + cell.h    },
        { cell.re0 + half_w,    cell.im0 + half_h    }
    };

    const double cell_radius = std::sqrt(half_w * half_w + half_h * half_h) * p.prune_margin;

    bool all_escaped = true;
    double min_dist = std::numeric_limits<double>::infinity();

    // _ChatGPT_ deep inside one attracting component stop
    bool all_strong_same = true;
    int common_period = -1;

    for (int i = 0; i < 5; ++i)
    {
        const Complex<flt> c((flt)sample[i][0], (flt)sample[i][1]);

        double dist = 0.0;
        const bool escaped = bl_escape_distance_estimate(c, p.prune_iter_max, p.escape_r2, dist);
        if (escaped)
            min_dist = std::min(min_dist, dist);
        else
            all_escaped = false;

        const int per = bl_detect_attracting_period(c, p.max_period, std::min(p.detect_iter_max, 700), p.detect_warmup, p.escape_r2, p.detect_eps2);

        if (per <= 0 || (!p.include_main && per == 1))
        {
            all_strong_same = false;
            continue;
        }

        if (common_period < 0)
            common_period = per;
        else if (common_period != per)
            all_strong_same = false;

        flt mult2 = (flt)0;
        const int settle = std::max(160, per * 18);
        if (!bl_estimate_cycle_multiplier2(c, per, settle, p.escape_r2, mult2))
            all_strong_same = false;

        const double mult = std::sqrt((double)mult2);
        if (mult > p.mult_strong)
            all_strong_same = false;
    }

    if (all_escaped && min_dist > cell_radius)
        return;

    if (all_strong_same && common_period > 0)
    {
        bl_try_seed_cell(cell, p, cusp_scale_min, dedup_cell, st);
        return;
    }

    // subdivide
    QuadCell c00{ cell.re0,          cell.im0,          half_w, half_h };
    QuadCell c10{ cell.re0 + half_w, cell.im0,          half_w, half_h };
    QuadCell c01{ cell.re0,          cell.im0 + half_h, half_w, half_h };
    QuadCell c11{ cell.re0 + half_w, cell.im0 + half_h, half_w, half_h };

    {
        std::lock_guard<std::mutex> lock(st.queue_mutex);
        st.queue.push_back(c00);
        st.queue.push_back(c10);
        st.queue.push_back(c01);
        st.queue.push_back(c11);
        st.pending.fetch_add(4, std::memory_order_relaxed);
    }

    st.queue_cv.notify_all();
}

static std::vector<BabyMandelEntry> build_baby_mandelbrot_atlas_f128_streaming(const BabyMandelParams& p)
{
    BabyMandelAtlasState st;

    const flt cusp_scale_min = flt(0.25) / p.max_zoom;
    const double cusp_scale_min_d = (double)cusp_scale_min;
    const double leaf_target = std::max(cusp_scale_min_d * std::max(0.05, p.leaf_factor), 1e-15);
    const double dedup_cell = std::max(cusp_scale_min_d * std::max(0.02, p.dedup_factor), 1e-15);

    st.seen.reserve((size_t)std::min(p.max_entries * 2, 400000));
    st.out.reserve((size_t)std::min(p.max_entries, 8192));

    QuadCell root;
    root.re0 = p.min_re;
    root.im0 = p.min_im;
    root.w = p.max_re - p.min_re;
    root.h = p.max_im - p.min_im;

    {
        std::lock_guard<std::mutex> lock(st.queue_mutex);
        st.queue.push_back(root);
        st.pending.store(1, std::memory_order_relaxed);
    }

    const int thread_count = std::max(1, (int)Thread::threadCount());

    std::vector<decltype(Thread::pool().submit_task([]() { return 0; }))> futures;
    futures.resize((size_t)thread_count);

    for (int ti = 0; ti < thread_count; ++ti)
    {
        futures[(size_t)ti] = Thread::pool().submit_task([&, ti]() -> int
        {
            (void)ti;

            while (true)
            {
                QuadCell cell;

                {
                    std::unique_lock<std::mutex> lock(st.queue_mutex);
                    st.queue_cv.wait(lock, [&]() { return st.done.load(std::memory_order_relaxed) || !st.queue.empty(); });

                    if (st.done.load(std::memory_order_relaxed) && st.queue.empty())
                        break;

                    if (st.queue.empty())
                        continue;

                    cell = st.queue.back();
                    st.queue.pop_back();
                }

                bl_process_cell(cell, p, leaf_target, cusp_scale_min, dedup_cell, st);

                const int64_t remain = st.pending.fetch_sub(1, std::memory_order_relaxed) - 1;
                if (remain == 0)
                {
                    st.done.store(true, std::memory_order_relaxed);
                    st.queue_cv.notify_all();
                    break;
                }
            }

            return 0;
        });
    }

    for (auto& fut : futures)
        fut.get();

    // _ChatGPT_ stable sort by zoom so UI can browse from large to small copies
    std::sort(st.out.begin(), st.out.end(), [](const BabyMandelEntry& a, const BabyMandelEntry& b)
    {
        return (double)a.zoom < (double)b.zoom;
    });

    return st.out;
}

static inline std::vector<BabyMandelEntry> build_baby_mandelbrot_atlas_f128(const BabyMandelParams& p)
{
    return build_baby_mandelbrot_atlas_f128_streaming(p);
}

SIM_END;