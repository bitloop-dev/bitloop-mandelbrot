#pragma once

#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include <mutex>
#include <atomic>

// _ChatGPT_ expects fdd, Complex<T>, Thread, and blPrint to be available from the including translation unit

using flt = f64;

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

struct BabyMandelEntry
{
    Complex<flt> center;
    Complex<flt> root;
    flt zoom;
    double rotation_rad;
    int period;
    flt scale_radius;
};

struct BabyMandelParams
{
    // _ChatGPT_ region to search in the parameter plane
    double min_re = -2.0;
    double max_re =  1.0;
    double min_im = -1.5;
    double max_im =  1.5;

    // _ChatGPT_ include copies whose cardioid scale >= 0.25 / max_zoom
    flt max_zoom = flt(100);

    // _ChatGPT_ recursion leaf size = leaf_factor * (0.25 / max_zoom)
    double leaf_factor = 0.40;

    // _ChatGPT_ per-leaf seed sampling grid
    int leaf_samples_axis = 3;

    // _ChatGPT_ period detection
    int max_period = 64;
    int detect_iter_max = 1400;
    int detect_warmup = 64;

    flt escape_r2 = flt(4.0);
    flt detect_eps2 = flt(1e-28);

    // _ChatGPT_ center Newton solve f_c^p(0)=0
    int center_newton_max_iters = 70;
    flt center_newton_tol2 = flt(1e-56);
    flt center_newton_min_deriv2 = flt(1e-90);

    // _ChatGPT_ fixed-c periodic point Newton solve f_c^p(z)=z
    int z_newton_max_iters = 50;
    flt z_newton_tol2 = flt(1e-56);
    flt z_newton_min_deriv2 = flt(1e-90);

    // _ChatGPT_ continuation to parabolic root by enforcing f_c^p(z)=z and (f_c^p)'(z)=lambda, lambda -> 1
    int cont_steps = 42;
    int cont_newton_max_iters = 40;
    flt cont_tol2 = flt(1e-54);
    flt cont_min_det2 = flt(1e-90);

    // _ChatGPT_ quadtree prune using escape distance estimate
    int prune_iter_max = 250;
    double prune_margin = 1.05;

    // _ChatGPT_ early stop inside a single attracting component when |mult| <= mult_strong
    double mult_strong = 0.60;

    // _ChatGPT_ dedup quantization relative to r_min
    double dedup_factor = 0.15;

    // _ChatGPT_ safety cap
    int max_entries = 200000;

    // _ChatGPT_ include the main cardioid as a trivial minibrot
    bool include_main = false;
};

struct QuadCell
{
    double re0 = 0;
    double im0 = 0;
    double w = 0;
    double h = 0;
};

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

static inline bool bl_cell_all_inside_fast(const QuadCell& cell, bool include_main, bool& out_all_cardioid, bool& out_all_bulb2)
{
    (void)include_main;
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
        // _ChatGPT_ dzdc_{n+1} = 2 z_n dzdc_n + 1
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

static bool bl_compute_center_newton(Complex<flt> c, int period, const BabyMandelParams& p, Complex<flt>& out_center)
{
    for (int it = 0; it < p.center_newton_max_iters; ++it)
    {
        Complex<flt> z((flt)0, (flt)0);
        Complex<flt> dzdc((flt)0, (flt)0);

        for (int i = 0; i < period; ++i)
        {
            dzdc = bl_complex_add_real(bl_complex_mul_scalar(z * dzdc, (flt)2), (flt)1);
            z = z.squared() + c;
        }

        if (bl_complex_mag2(z) < p.center_newton_tol2)
        {
            out_center = c;
            return true;
        }

        if (bl_complex_mag2(dzdc) < p.center_newton_min_deriv2)
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

        if (bl_complex_mag2(z) < tol2)
            return d;
    }

    return p;
}

static bool bl_refine_periodic_point_fixed_c(
    const Complex<flt>& c,
    int period,
    const BabyMandelParams& p,
    Complex<flt>& z)
{
    for (int it = 0; it < p.z_newton_max_iters; ++it)
    {
        Complex<flt> zn = z;
        Complex<flt> A((flt)1, (flt)0);

        for (int i = 0; i < period; ++i)
        {
            A = bl_complex_mul_scalar(zn * A, (flt)2);
            zn = zn.squared() + c;
        }

        const Complex<flt> F = zn - z;
        if (bl_complex_mag2(F) < p.z_newton_tol2)
            return true;

        const Complex<flt> dF = A - Complex<flt>((flt)1, (flt)0);
        if (bl_complex_mag2(dF) < p.z_newton_min_deriv2)
            return false;

        z = z - (F / dF);
    }

    return false;
}

static bool bl_solve_2x2_complex(
    const Complex<flt>& a, const Complex<flt>& b,
    const Complex<flt>& c, const Complex<flt>& d,
    const Complex<flt>& rhs1, const Complex<flt>& rhs2,
    flt min_det2,
    Complex<flt>& out_x, Complex<flt>& out_y)
{
    const Complex<flt> det = a * d - b * c;
    if (bl_complex_mag2(det) < min_det2)
        return false;

    out_x = (rhs1 * d - b * rhs2) / det;
    out_y = (a * rhs2 - rhs1 * c) / det;
    return true;
}

static int bl_minimal_period_of_point(const Complex<flt>& c, const Complex<flt>& z0, int p, flt tol2)
{
    for (int d = 1; d < p; ++d)
    {
        if ((p % d) != 0)
            continue;

        Complex<flt> z = z0;
        for (int i = 0; i < d; ++i)
            z = z.squared() + c;

        if (bl_complex_mag2(z - z0) < tol2)
            return d;
    }

    return p;
}

static bool bl_continue_to_root_exact_period(
    Complex<flt> center,
    int period,
    const BabyMandelParams& p,
    Complex<flt>& out_root,
    Complex<flt>& out_parabolic_point)
{
    // _ChatGPT_ seed z from the attracting cycle at the center
    Complex<flt> z((flt)0, (flt)0);
    const int warm = std::max(96, period * 10);
    for (int i = 0; i < warm; ++i)
        z = z.squared() + center;

    if (!bl_refine_periodic_point_fixed_c(center, period, p, z))
        return false;

    Complex<flt> c = center;

    for (int step = 1; step <= p.cont_steps; ++step)
    {
        const double t = (double)step / (double)p.cont_steps;
        const double eased = t * (2.0 - t);
        const flt lambda_target = flt(eased);
        const Complex<flt> lambda_c(lambda_target, (flt)0);

        for (int it = 0; it < p.cont_newton_max_iters; ++it)
        {
            Complex<flt> zn = z;

            Complex<flt> A((flt)1, (flt)0);
            Complex<flt> B((flt)0, (flt)0);

            Complex<flt> C((flt)0, (flt)0);
            Complex<flt> D((flt)0, (flt)0);

            for (int i = 0; i < period; ++i)
            {
                // _ChatGPT_ second-order recurrences for multiplier derivatives
                const Complex<flt> C_next = bl_complex_mul_scalar(A * A, (flt)2) + bl_complex_mul_scalar(zn * C, (flt)2);
                const Complex<flt> D_next = bl_complex_mul_scalar(B * A, (flt)2) + bl_complex_mul_scalar(zn * D, (flt)2);

                const Complex<flt> B_next = bl_complex_add_real(bl_complex_mul_scalar(zn * B, (flt)2), (flt)1);
                const Complex<flt> A_next = bl_complex_mul_scalar(zn * A, (flt)2);

                zn = zn.squared() + c;

                A = A_next;
                B = B_next;
                C = C_next;
                D = D_next;
            }

            const Complex<flt> F1 = (zn - z);
            const Complex<flt> F2 = (A - lambda_c);

            if (bl_complex_mag2(F1) + bl_complex_mag2(F2) < p.cont_tol2)
                break;

            const Complex<flt> J11 = (A - Complex<flt>((flt)1, (flt)0));
            const Complex<flt> J12 = B;
            const Complex<flt> J21 = C;
            const Complex<flt> J22 = D;

            Complex<flt> dz, dc;
            if (!bl_solve_2x2_complex(J11, J12, J21, J22, -F1, -F2, p.cont_min_det2, dz, dc))
                return false;

            z = z + dz;
            c = c + dc;
        }
    }

    out_root = c;
    out_parabolic_point = z;

    // _ChatGPT_ reject satellites by requiring the parabolic cycle to have exact period
    const flt per_tol2 = p.cont_tol2 * (flt)1000;
    if (bl_minimal_period_of_point(out_root, out_parabolic_point, period, per_tol2) != period)
        return false;

    return true;
}

struct RootKey
{
    int64_t qx = 0;
    int64_t qy = 0;
    int period = 0;

    bool operator==(const RootKey& o) const { return qx == o.qx && qy == o.qy && period == o.period; }
};

struct RootKeyHash
{
    size_t operator()(const RootKey& k) const
    {
        uint64_t h = (uint64_t)k.qx * 0x9E3779B185EBCA87ull;
        h ^= (uint64_t)k.qy + 0xC2B2AE3D27D4EB4Full + (h << 6) + (h >> 2);
        h ^= (uint64_t)(uint32_t)k.period + 0x165667B19E3779F9ull + (h << 6) + (h >> 2);
        return (size_t)h;
    }
};

static RootKey bl_make_root_key(const Complex<flt>& r, int period, double cell)
{
    RootKey k;
    k.qx = (int64_t)std::llround((double)r.x / cell);
    k.qy = (int64_t)std::llround((double)r.y / cell);
    k.period = period;
    return k;
}

static inline double bl_cell_radius_est(const QuadCell& cell, double margin)
{
    const double half_w = 0.5 * cell.w;
    const double half_h = 0.5 * cell.h;
    return std::sqrt(half_w * half_w + half_h * half_h) * margin;
}

static inline QuadCell bl_subcell(const QuadCell& cell, int ix, int iy)
{
    const double half_w = 0.5 * cell.w;
    const double half_h = 0.5 * cell.h;

    QuadCell c;
    c.re0 = cell.re0 + (double)ix * half_w;
    c.im0 = cell.im0 + (double)iy * half_h;
    c.w = half_w;
    c.h = half_h;
    return c;
}

static inline void bl_split_for_tasks(const BabyMandelParams& p, std::vector<QuadCell>& out_cells)
{
    QuadCell root;
    root.re0 = p.min_re;
    root.im0 = p.min_im;
    root.w = p.max_re - p.min_re;
    root.h = p.max_im - p.min_im;

    const double max_dim = std::max(root.w, root.h);
    const double task_target = std::max(0.18, max_dim / 8.0);

    std::vector<QuadCell> stack;
    stack.push_back(root);

    while (!stack.empty())
    {
        const QuadCell cell = stack.back();
        stack.pop_back();

        if (std::max(cell.w, cell.h) <= task_target)
        {
            out_cells.push_back(cell);
            continue;
        }

        stack.push_back(bl_subcell(cell, 0, 0));
        stack.push_back(bl_subcell(cell, 1, 0));
        stack.push_back(bl_subcell(cell, 0, 1));
        stack.push_back(bl_subcell(cell, 1, 1));
    }
}

struct BabyMandelStreamState
{
    std::mutex mutex;
    std::unordered_set<RootKey, RootKeyHash> seen;
    std::vector<BabyMandelEntry> out;
    std::atomic<int> accepted_count{ 0 };
};

static inline bool bl_try_accept_entry(
    const BabyMandelParams& p,
    BabyMandelStreamState& state,
    const BabyMandelEntry& e,
    double dedup_cell)
{
    const RootKey k = bl_make_root_key(e.root, e.period, dedup_cell);

    {
        std::scoped_lock lock(state.mutex);

        if ((int)state.out.size() >= p.max_entries)
            return false;

        if (state.seen.find(k) != state.seen.end())
            return false;

        state.seen.insert(k);
        state.out.push_back(e);
        state.accepted_count.store((int)state.out.size(), std::memory_order_relaxed);

        blPrint() << "Accepted minibrot: center=" << e.center << " root=" << e.root
            << " period=" << e.period << " zoom=" << e.zoom << "\n";
    }

    return true;
}

static inline void bl_attempt_discover_from_seed(
    const BabyMandelParams& p,
    BabyMandelStreamState& state,
    const Complex<flt>& seed,
    double r_min_d,
    double dedup_cell)
{
    if ((int)state.accepted_count.load(std::memory_order_relaxed) >= p.max_entries)
        return;

    const int per = bl_detect_attracting_period(seed, p.max_period, p.detect_iter_max, p.detect_warmup, p.escape_r2, p.detect_eps2);
    if (per <= 0)
        return;

    if (!p.include_main && per == 1)
        return;

    Complex<flt> center;
    if (!bl_compute_center_newton(seed, per, p, center))
        return;

    const int per_min = bl_minimal_period_at_center(center, per, p.center_newton_tol2);
    if (per_min != per)
        return;

    Complex<flt> root;
    Complex<flt> zpar;
    if (!bl_continue_to_root_exact_period(center, per, p, root, zpar))
        return;

    const Complex<flt> d = root - center;
    const flt rad = d.mag();
    const double rr = (double)rad;
    if (!(rr > 0.0) || rr < r_min_d)
        return;

    BabyMandelEntry e;
    e.center = center;
    e.root = root;
    e.scale_radius = rad;
    e.zoom = flt(0.25) / rad;

    const Complex<double> dd = (Complex<double>)d;
    e.rotation_rad = -std::atan2(dd.y, dd.x);
    e.period = per;

    bl_try_accept_entry(p, state, e, dedup_cell);
}

static inline bool bl_cell_provably_outside(const BabyMandelParams& p, const QuadCell& cell)
{
    const double half_w = 0.5 * cell.w;
    const double half_h = 0.5 * cell.h;

    const double sample[5][2] =
    {
        { cell.re0,             cell.im0             },
        { cell.re0 + cell.w,    cell.im0             },
        { cell.re0,             cell.im0 + cell.h    },
        { cell.re0 + cell.w,    cell.im0 + cell.h    },
        { cell.re0 + half_w,    cell.im0 + half_h    }
    };

    const double cell_radius = bl_cell_radius_est(cell, p.prune_margin);

    bool all_escaped = true;
    double min_dist = std::numeric_limits<double>::infinity();

    for (int i = 0; i < 5; ++i)
    {
        const Complex<flt> c((flt)sample[i][0], (flt)sample[i][1]);
        double dist = 0.0;
        const bool escaped = bl_escape_distance_estimate(c, p.prune_iter_max, p.escape_r2, dist);
        if (!escaped)
            all_escaped = false;
        else
            min_dist = std::min(min_dist, dist);
    }

    return all_escaped && (min_dist > cell_radius);
}

static inline bool bl_cell_deep_inside_single_component(
    const BabyMandelParams& p,
    const QuadCell& cell,
    int& out_period)
{
    out_period = 0;

    const int classify_iter_max = std::min(p.detect_iter_max, 700);
    const flt mult2_strong = (flt)(p.mult_strong * p.mult_strong);

    const double half_w = 0.5 * cell.w;
    const double half_h = 0.5 * cell.h;

    const double sample[5][2] =
    {
        { cell.re0,             cell.im0             },
        { cell.re0 + cell.w,    cell.im0             },
        { cell.re0,             cell.im0 + cell.h    },
        { cell.re0 + cell.w,    cell.im0 + cell.h    },
        { cell.re0 + half_w,    cell.im0 + half_h    }
    };

    bool all_strong_same = true;
    int common_period = -1;

    for (int i = 0; i < 5; ++i)
    {
        const Complex<flt> c((flt)sample[i][0], (flt)sample[i][1]);

        const int per = bl_detect_attracting_period(c, p.max_period, classify_iter_max, p.detect_warmup, p.escape_r2, p.detect_eps2);
        if (per <= 0 || (!p.include_main && per == 1))
            return false;

        if (common_period < 0)
            common_period = per;
        else if (common_period != per)
            all_strong_same = false;

        flt mult2 = (flt)0;
        const int settle = std::max(128, per * 16);
        if (!bl_estimate_cycle_multiplier2(c, per, settle, p.escape_r2, mult2))
            return false;

        if (mult2 > mult2_strong)
            all_strong_same = false;
    }

    if (!all_strong_same || common_period <= 0)
        return false;

    out_period = common_period;
    return true;
}

static inline void bl_recurse_cell(
    const BabyMandelParams& p,
    BabyMandelStreamState& state,
    const QuadCell& cell,
    double leaf_target,
    double r_min_d,
    double dedup_cell)
{
    if ((int)state.accepted_count.load(std::memory_order_relaxed) >= p.max_entries)
        return;

    if (std::max(cell.w, cell.h) <= leaf_target)
    {
        for (int sy = 0; sy < p.leaf_samples_axis; ++sy)
        {
            for (int sx = 0; sx < p.leaf_samples_axis; ++sx)
            {
                const double fx = ((double)sx + 0.5) / (double)p.leaf_samples_axis;
                const double fy = ((double)sy + 0.5) / (double)p.leaf_samples_axis;

                const double re = cell.re0 + fx * cell.w;
                const double im = cell.im0 + fy * cell.h;

                bl_attempt_discover_from_seed(p, state, Complex<flt>((flt)re, (flt)im), r_min_d, dedup_cell);
            }
        }
        return;
    }

    if (bl_cell_provably_outside(p, cell))
        return;

    bool all_cardioid = false;
    bool all_bulb2 = false;
    if (bl_cell_all_inside_fast(cell, p.include_main, all_cardioid, all_bulb2))
    {
        // _ChatGPT_ main cardioid or 2-bulb interiors contain no other component boundaries
        if (all_cardioid && p.include_main)
        {
            const double re = cell.re0 + 0.5 * cell.w;
            const double im = cell.im0 + 0.5 * cell.h;
            bl_attempt_discover_from_seed(p, state, Complex<flt>((flt)re, (flt)im), r_min_d, dedup_cell);
        }
        return;
    }

    int deep_period = 0;
    if (bl_cell_deep_inside_single_component(p, cell, deep_period))
    {
        // _ChatGPT_ attempt one discovery from the center so components are not missed by boundary-only sampling
        const double re = cell.re0 + 0.5 * cell.w;
        const double im = cell.im0 + 0.5 * cell.h;
        bl_attempt_discover_from_seed(p, state, Complex<flt>((flt)re, (flt)im), r_min_d, dedup_cell);
        return;
    }

    bl_recurse_cell(p, state, bl_subcell(cell, 0, 0), leaf_target, r_min_d, dedup_cell);
    bl_recurse_cell(p, state, bl_subcell(cell, 1, 0), leaf_target, r_min_d, dedup_cell);
    bl_recurse_cell(p, state, bl_subcell(cell, 0, 1), leaf_target, r_min_d, dedup_cell);
    bl_recurse_cell(p, state, bl_subcell(cell, 1, 1), leaf_target, r_min_d, dedup_cell);
}

static inline std::vector<BabyMandelEntry> build_baby_minibrot_atlas_streaming(const BabyMandelParams& p)
{
    const flt r_min = flt(0.25) / p.max_zoom;
    const double r_min_d = (double)r_min;

    const double leaf_target = std::max(r_min_d * std::max(0.05, p.leaf_factor), 1e-15);
    const double dedup_cell = std::max(r_min_d * std::max(0.02, p.dedup_factor), 1e-15);

    std::vector<QuadCell> task_cells;
    task_cells.reserve(512);
    bl_split_for_tasks(p, task_cells);

    BabyMandelStreamState state;
    state.seen.reserve((size_t)std::min(p.max_entries * 2, 400000));
    state.out.reserve((size_t)std::min(p.max_entries, 8192));

    const int thread_count = std::max(1, (int)Thread::threadCount());
    const int task_count = (int)task_cells.size();
    const int block = (task_count + thread_count - 1) / thread_count;

    using FutureType = decltype(Thread::pool().submit_task([]() { return 0; }));
    std::vector<FutureType> futures;
    futures.resize((size_t)thread_count);

    for (int ti = 0; ti < thread_count; ++ti)
    {
        futures[(size_t)ti] = Thread::pool().submit_task([&, ti]()
        {
            const int i0 = ti * block;
            const int i1 = std::min(task_count, i0 + block);

            for (int i = i0; i < i1; ++i)
            {
                if ((int)state.accepted_count.load(std::memory_order_relaxed) >= p.max_entries)
                    break;

                bl_recurse_cell(p, state, task_cells[(size_t)i], leaf_target, r_min_d, dedup_cell);
            }

            return 0;
        });
    }

    for (auto& fut : futures)
        fut.get();

    {
        std::scoped_lock lock(state.mutex);
        blPrint() << "Minibrot scan complete: accepted=" << (int)state.out.size() << "\n";
    }

    {
        std::scoped_lock lock(state.mutex);
        std::sort(state.out.begin(), state.out.end(), [](const BabyMandelEntry& a, const BabyMandelEntry& b)
        {
            return (double)a.zoom < (double)b.zoom;
        });
    }

    return state.out;
}
