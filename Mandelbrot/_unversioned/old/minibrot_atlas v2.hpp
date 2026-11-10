#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>

// Assumes your Complex<T> is available
// Assumes Thread::pool().submit_task and Thread::threadCount exist

typedef f64 flt;

template<typename T>
static constexpr Complex<T> complex_add_real(const Complex<T>& z, T r) { return Complex<T>(z.x + r, z.y); }

template<typename T>
static constexpr Complex<T> complex_mul_scalar(const Complex<T>& z, T s) { return Complex<T>(z.x * s, z.y * s); }

template<typename T>
static constexpr T complex_mag2(const Complex<T>& z) { return z.x * z.x + z.y * z.y; }

template<typename T>
static constexpr T complex_dist2(const Complex<T>& a, const Complex<T>& b)
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
    // region to search (parameter plane)
    double min_re = -2.0;
    double max_re = 1.0;
    double min_im = -1.5;
    double max_im = 1.5;

    // target coverage: include copies whose main cardioid scale >= 0.25 / max_zoom
    flt max_zoom = flt(30);

    // leaf size = leaf_factor * (0.25 / max_zoom)
    double leaf_factor = 0.40;

    // per-leaf sampling pattern: 2x2 = 4 seeds
    int leaf_samples_axis = 2;

    // period detection
    int max_period = 64;
    int detect_iter_max = 1200;
    int detect_warmup = 64;

    flt escape_r2 = flt(4.0);
    flt detect_eps2 = flt(1e-28);

    // center Newton: solve f_c^p(0)=0
    int center_newton_max_iters = 60;
    flt center_newton_tol2 = flt(1e-56);
    flt center_newton_min_deriv2 = flt(1e-90);

    // periodic-point Newton at fixed c: solve f_c^p(z)=z
    int z_newton_max_iters = 40;
    flt z_newton_tol2 = flt(1e-56);
    flt z_newton_min_deriv2 = flt(1e-90);

    // continuation to primitive cusp: solve f_c^p(z)=z and (f_c^p)'(z)=lambda, lambda -> 1
    int cont_steps = 28;
    int cont_newton_max_iters = 30;
    flt cont_tol2 = flt(1e-54);
    flt cont_min_det2 = flt(1e-90);

    // quadtree pruning using escape distance
    int prune_iter_max = 200;
    double prune_margin = 1.05;

    // dedup quantization relative to r_min
    double dedup_factor = 0.20;

    // safety: cap output
    int max_entries = 10000;

    // exclude main cardioid
    bool include_main = false;
};

static int detect_attracting_period(
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

static bool estimate_cycle_multiplier2(
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

static bool compute_center_newton(Complex<flt> c, int period, const BabyMandelParams& p, Complex<flt>& out_center)
{
    for (int it = 0; it < p.center_newton_max_iters; ++it)
    {
        Complex<flt> z((flt)0, (flt)0);
        Complex<flt> dzdc((flt)0, (flt)0);

        for (int i = 0; i < period; ++i)
        {
            // _ChatGPT_ dzdc_{n+1} = 2 z_n dzdc_n + 1
            dzdc = complex_add_real(complex_mul_scalar(z * dzdc, (flt)2), (flt)1);
            z = z.squared() + c;
        }

        if (complex_mag2(z) < p.center_newton_tol2)
        {
            out_center = c;
            return true;
        }

        if (complex_mag2(dzdc) < p.center_newton_min_deriv2)
            return false;

        c = c - (z / dzdc);
    }

    return false;
}

static int minimal_period_at_center(const Complex<flt>& c, int p, flt tol2)
{
    for (int d = 1; d < p; ++d)
    {
        if ((p % d) != 0)
            continue;

        Complex<flt> z((flt)0, (flt)0);
        for (int i = 0; i < d; ++i)
            z = z.squared() + c;

        if (complex_mag2(z) < tol2)
            return d;
    }
    return p;
}

static bool refine_periodic_point_fixed_c(
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
            A = complex_mul_scalar(zn * A, (flt)2);
            zn = zn.squared() + c;
        }

        const Complex<flt> F = zn - z;
        if (complex_mag2(F) < p.z_newton_tol2)
            return true;

        const Complex<flt> dF = A - Complex<flt>((flt)1, (flt)0);
        if (complex_mag2(dF) < p.z_newton_min_deriv2)
            return false;

        z = z - (F / dF);
    }

    return false;
}

static bool solve_2x2_complex(
    const Complex<flt>& a, const Complex<flt>& b,
    const Complex<flt>& c, const Complex<flt>& d,
    const Complex<flt>& rhs1, const Complex<flt>& rhs2,
    flt min_det2,
    Complex<flt>& out_x, Complex<flt>& out_y)
{
    const Complex<flt> det = a * d - b * c;
    if (complex_mag2(det) < min_det2)
        return false;

    out_x = (rhs1 * d - b * rhs2) / det;
    out_y = (a * rhs2 - rhs1 * c) / det;
    return true;
}

static bool continue_to_primitive_root(
    Complex<flt> center,
    int period,
    const BabyMandelParams& p,
    Complex<flt>& out_root)
{
    // _ChatGPT_ seed z from the attracting cycle at the center
    Complex<flt> z((flt)0, (flt)0);
    const int warm = std::max(64, period * 8);
    for (int i = 0; i < warm; ++i)
        z = z.squared() + center;

    if (!refine_periodic_point_fixed_c(center, period, p, z))
        return false;

    Complex<flt> c = center;

    for (int step = 1; step <= p.cont_steps; ++step)
    {
        const flt lambda_target = flt((double)step / (double)p.cont_steps);
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
                const Complex<flt> C_next = complex_mul_scalar(A * A, (flt)2) + complex_mul_scalar(zn * C, (flt)2);
                const Complex<flt> D_next = complex_mul_scalar(B * A, (flt)2) + complex_mul_scalar(zn * D, (flt)2);

                const Complex<flt> B_next = complex_add_real(complex_mul_scalar(zn * B, (flt)2), (flt)1);
                const Complex<flt> A_next = complex_mul_scalar(zn * A, (flt)2);

                zn = zn.squared() + c;

                A = A_next;
                B = B_next;
                C = C_next;
                D = D_next;
            }

            const Complex<flt> F1 = (zn - z);
            const Complex<flt> F2 = (A - lambda_c);

            if (complex_mag2(F1) + complex_mag2(F2) < p.cont_tol2)
                break;

            const Complex<flt> J11 = (A - Complex<flt>((flt)1, (flt)0)); // dF1/dz
            const Complex<flt> J12 = B;                                     // dF1/dc
            const Complex<flt> J21 = C;                                     // dF2/dz
            const Complex<flt> J22 = D;                                     // dF2/dc

            Complex<flt> dz, dc;
            if (!solve_2x2_complex(J11, J12, J21, J22, -F1, -F2, p.cont_min_det2, dz, dc))
                return false;

            z = z + dz;
            c = c + dc;
        }
    }

    // _ChatGPT_ multiplier is forced to +1 along the real lambda continuation
    out_root = c;
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

static RootKey make_root_key(const Complex<flt>& r, int period, double cell)
{
    RootKey k;
    k.qx = (int64_t)std::llround((double)r.x / cell);
    k.qy = (int64_t)std::llround((double)r.y / cell);
    k.period = period;
    return k;
}

struct QuadCell
{
    double re0 = 0;
    double im0 = 0;
    double w = 0;
    double h = 0;
};

static bool escape_distance_estimate(
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

static std::vector<QuadCell> build_quadtree_leaves(const BabyMandelParams& p)
{
    const flt r_min = flt(0.25) / p.max_zoom;
    const double r_min_d = (double)r_min;

    const double leaf_target = std::max(r_min_d * std::max(0.05, p.leaf_factor), 1e-15);

    std::vector<QuadCell> leaves;
    std::vector<QuadCell> stack;
    stack.reserve(1 << 16);

    QuadCell root;
    root.re0 = p.min_re;
    root.im0 = p.min_im;
    root.w = p.max_re - p.min_re;
    root.h = p.max_im - p.min_im;
    stack.push_back(root);

    const int classify_iter_max = std::min(p.detect_iter_max, 700);
    const flt mult2_strong = (flt)0.25; // |mult| <= 0.5

    while (!stack.empty())
    {
        const QuadCell cell = stack.back();
        stack.pop_back();

        const double max_dim = std::max(cell.w, cell.h);
        if (max_dim <= leaf_target)
        {
            leaves.push_back(cell);
            continue;
        }

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

        bool all_strong_same = true;
        int common_period = -1;

        for (int i = 0; i < 5; ++i)
        {
            const Complex<flt> c((flt)sample[i][0], (flt)sample[i][1]);

            double dist = 0.0;
            const bool escaped = escape_distance_estimate(c, p.prune_iter_max, p.escape_r2, dist);
            if (escaped)
                min_dist = std::min(min_dist, dist);
            else
                all_escaped = false;

            const int per = detect_attracting_period(
                c, p.max_period,
                classify_iter_max,
                p.detect_warmup,
                p.escape_r2,
                p.detect_eps2);

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
            const int settle = std::max(128, per * 16);
            if (!estimate_cycle_multiplier2(c, per, settle, p.escape_r2, mult2))
                all_strong_same = false;

            if (mult2 > mult2_strong)
                all_strong_same = false;
        }

        if (all_escaped && min_dist > cell_radius)
            continue;

        if (all_strong_same && common_period > 0)
        {
            // _ChatGPT_ deep inside one attracting component, no need to subdivide further
            leaves.push_back(cell);
            continue;
        }

        const QuadCell c00{ cell.re0,          cell.im0,          half_w, half_h };
        const QuadCell c10{ cell.re0 + half_w, cell.im0,          half_w, half_h };
        const QuadCell c01{ cell.re0,          cell.im0 + half_h, half_w, half_h };
        const QuadCell c11{ cell.re0 + half_w, cell.im0 + half_h, half_w, half_h };

        stack.push_back(c00);
        stack.push_back(c10);
        stack.push_back(c01);
        stack.push_back(c11);
    }

    return leaves;
}

static std::vector<BabyMandelEntry> build_baby_mandelbrot_atlas_f128(const BabyMandelParams& p)
{
    const flt r_min = flt(0.25) / p.max_zoom;
    const double r_min_d = (double)r_min;

    const std::vector<QuadCell> leaves = build_quadtree_leaves(p);

    const int thread_count = std::max(1, (int)Thread::threadCount());
    const int leaf_count = (int)leaves.size();
    const int block = (leaf_count + thread_count - 1) / thread_count;

    std::vector<decltype(Thread::pool().submit_task([]() { return std::vector<BabyMandelEntry>{}; })) > futures;
    futures.resize((size_t)thread_count);

    for (int ti = 0; ti < thread_count; ++ti)
    {
        futures[(size_t)ti] = Thread::pool().submit_task([&, ti]()
        {
            std::vector<BabyMandelEntry> local;
            local.reserve(256);

            const int i0 = ti * block;
            const int i1 = std::min(leaf_count, i0 + block);

            for (int i = i0; i < i1; ++i)
            {
                const QuadCell cell = leaves[(size_t)i];

                // _ChatGPT_ 2x2 interior samples reduces misses when a small cardioid sits between cell centers
                for (int sy = 0; sy < p.leaf_samples_axis; ++sy)
                    for (int sx = 0; sx < p.leaf_samples_axis; ++sx)
                    {
                        const double fx = ((double)sx + 0.5) / (double)p.leaf_samples_axis;
                        const double fy = ((double)sy + 0.5) / (double)p.leaf_samples_axis;

                        const double re = cell.re0 + fx * cell.w;
                        const double im = cell.im0 + fy * cell.h;

                        const Complex<flt> seed((flt)re, (flt)im);

                        const int per = detect_attracting_period(
                            seed,
                            p.max_period,
                            p.detect_iter_max,
                            p.detect_warmup,
                            p.escape_r2,
                            p.detect_eps2);

                        if (per <= 0)
                            continue;

                        if (!p.include_main && per == 1)
                            continue;

                        Complex<flt> center;
                        if (!compute_center_newton(seed, per, p, center))
                            continue;

                        const int per_min = minimal_period_at_center(center, per, p.center_newton_tol2);
                        if (per_min != per)
                            continue;

                        Complex<flt> root;
                        if (!continue_to_primitive_root(center, per, p, root))
                            continue;

                        const Complex<flt> d = root - center;
                        const flt r2 = complex_mag2(d);
                        if (r2 <= (flt)0)
                            continue;

                        const double rr = std::sqrt((double)r2);
                        if (rr < r_min_d)
                            continue;

                        BabyMandelEntry e;
                        e.center = center;
                        e.root = root;
                        e.scale_radius = (flt)rr;
                        e.zoom = flt(0.25) / e.scale_radius;

                        const Complex<double> dd = (Complex<double>)d;
                        e.rotation_rad = -std::atan2(dd.y, dd.x);
                        e.period = per;

                        //blPrint() << "Found baby: center=" << center << " root=" << root << " period=" << per << " zoom=" << e.zoom << "\n";

                        local.push_back(e);
                        if ((int)local.size() >= p.max_entries)
                            return local;
                    }
            }

            return local;
        });
    }

    // merge + dedup by quantized root
    const double dedup_cell = std::max(r_min_d * std::max(0.02, p.dedup_factor), 1e-15);

    std::unordered_set<RootKey, RootKeyHash> seen;
    seen.reserve((size_t)std::min(p.max_entries * 2, 200000));

    std::vector<BabyMandelEntry> out;
    out.reserve((size_t)std::min(p.max_entries, 8192));

    for (auto& fut : futures)
    {
        auto local = fut.get();
        for (const BabyMandelEntry& e : local)
        {
            if ((int)out.size() >= p.max_entries)
                break;

            const RootKey k = make_root_key(e.root, e.period, dedup_cell);
            if (seen.find(k) != seen.end())
                continue;

            seen.insert(k);
            out.push_back(e);

            blPrint() << "Accepted baby: center=" << e.center << " root=" << e.root << " period=" << e.period << " zoom=" << e.zoom << "\n";
        }
        if ((int)out.size() >= p.max_entries)
            break;
    }

    // _ChatGPT_ stable sort by zoom so UI can browse from large to small copies
    std::sort(out.begin(), out.end(), [](const BabyMandelEntry& a, const BabyMandelEntry& b)
    {
        return (double)a.zoom < (double)b.zoom;
    });

    return out;
}
