#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>

template<typename T>
static constexpr T complex_mag2(const Complex<T>& z) { return z.x * z.x + z.y * z.y; }

template<typename T>
static constexpr T complex_dist2(const Complex<T>& a, const Complex<T>& b)
{
    const T dx = a.x - b.x;
    const T dy = a.y - b.y;
    return dx * dx + dy * dy;
}

template<typename T>
static constexpr Complex<T> complex_mul_scalar(const Complex<T>& z, T s) { return Complex<T>(z.x * s, z.y * s); }

template<typename T>
static constexpr Complex<T> complex_add_real(const Complex<T>& z, T r) { return Complex<T>(z.x + r, z.y); }

struct MinibrotAtlasEntry
{
    Complex<fdd> center;
    Complex<fdd> cusp_like;
    fdd zoom;
    double rotation_rad;
    int period;
    fdd est_radius;
};

struct MinibrotAtlasParams
{
    int max_period = 18;

    int detect_iter_max = 1500;
    int detect_warmup = 64;

    fdd escape_r2 = fdd(4.0);
    fdd detect_eps2 = fdd(1e-28);

    int newton_max_iters = 50;
    fdd newton_tol2 = fdd(1e-56);
    fdd newton_min_deriv2 = fdd(1e-80);

    int boundary_dirs = 32;
    int boundary_binary_steps = 80;
    int boundary_iter_max = 2500;
    fdd boundary_eps2 = fdd(1e-24);

    fdd boundary_r_start = fdd(1e-6);
    fdd boundary_r_max = fdd(2.5);

    // parameter-plane scan box
    double scan_min_re = -2.0;
    double scan_max_re = 1.0;
    double scan_min_im = -1.5;
    double scan_max_im = 1.5;
};

struct MinibrotAtlasScanParams
{
    MinibrotAtlasParams algo;

    // include components whose estimated cardioid radius >= 0.25 / zoom_max
    fdd zoom_max = fdd(10);

    // grid spacing = step_factor * r_min at the finest level
    double step_factor = 0.50;

    // progressive refinement start step is coarse and halves until finest
    bool progressive = true;

    // avoids infinite output if zoom_max is small
    int max_entries = 10000;

    // exclude main cardioid (period 1) by default
    bool include_main = true;
};

static int detect_attracting_period(
    const Complex<fdd>& c,
    int max_period,
    int iter_max,
    int warmup,
    fdd escape_r2,
    fdd eps2)
{
    const int ring_size = std::max(8, max_period * 3);
    std::vector<Complex<fdd>> ring((size_t)ring_size, Complex<fdd>((fdd)0, (fdd)0));

    Complex<fdd> z((fdd)0, (fdd)0);

    for (int n = 0; n < iter_max; ++n)
    {
        z = z.squared() + c;
        ring[(size_t)(n % ring_size)] = z;

        if (complex_mag2(z) > escape_r2)
            return 0;

        if (n < warmup)
            continue;

        for (int p = 1; p <= max_period; ++p)
        {
            if (n < 2 * p)
                continue;

            const Complex<fdd>& z1 = ring[(size_t)((n - p) % ring_size)];
            const Complex<fdd>& z2 = ring[(size_t)((n - 2 * p) % ring_size)];

            if (complex_dist2(z, z1) < eps2 && complex_dist2(z, z2) < eps2 * (fdd)4)
                return p;
        }
    }

    return 0;
}

static bool compute_center_newton(
    Complex<fdd> c,
    int period,
    int max_iters,
    fdd tol2,
    fdd min_deriv2,
    Complex<fdd>& out_center)
{
    for (int it = 0; it < max_iters; ++it)
    {
        Complex<fdd> z((fdd)0, (fdd)0);
        Complex<fdd> dzdc((fdd)0, (fdd)0);

        for (int i = 0; i < period; ++i)
        {
            // _ChatGPT_ dzdc_{n+1} = 2 z_n dzdc_n + 1
            dzdc = complex_add_real(complex_mul_scalar(z * dzdc, (fdd)2), (fdd)1);
            z = z.squared() + c;
        }

        if (complex_mag2(z) < tol2)
        {
            out_center = c;
            return true;
        }

        if (complex_mag2(dzdc) < min_deriv2)
            return false;

        c = c - (z / dzdc);
    }

    return false;
}

static int minimal_period_at_center(const Complex<fdd>& c, int p, fdd tol2)
{
    for (int d = 1; d < p; ++d)
    {
        if ((p % d) != 0)
            continue;

        Complex<fdd> z((fdd)0, (fdd)0);
        for (int i = 0; i < d; ++i)
            z = z.squared() + c;

        if (complex_mag2(z) < tol2)
            return d;
    }

    return p;
}

static bool in_component_period(
    const Complex<fdd>& c,
    int desired_period,
    const MinibrotAtlasParams& params)
{
    const int p = detect_attracting_period(
        c,
        params.max_period,
        params.boundary_iter_max,
        params.detect_warmup,
        params.escape_r2,
        params.boundary_eps2);

    return p == desired_period;
}

static void estimate_cusp_like_and_radius(
    const Complex<fdd>& center,
    int period,
    const MinibrotAtlasParams& params,
    Complex<fdd>& out_cusp_like,
    fdd& out_radius)
{
    const double two_pi = 6.283185307179586476925286766559;
    fdd best_r = (fdd)0;
    Complex<fdd> best_dir((fdd)1, (fdd)0);

    for (int k = 0; k < params.boundary_dirs; ++k)
    {
        const double ang = two_pi * (double)k / (double)params.boundary_dirs;
        const fdd dx = (fdd)std::cos(ang);
        const fdd dy = (fdd)std::sin(ang);
        const Complex<fdd> dir(dx, dy);

        fdd r_in = (fdd)0;
        fdd r_out = params.boundary_r_start;

        auto inside = [&](fdd r) -> bool
        {
            const Complex<fdd> c = center + complex_mul_scalar(dir, r);
            return in_component_period(c, period, params);
        };

        if (!inside(r_out))
            continue;

        while (r_out < params.boundary_r_max && inside(r_out))
        {
            r_in = r_out;
            r_out = r_out * (fdd)2;
        }

        if (r_out >= params.boundary_r_max)
            continue;

        for (int s = 0; s < params.boundary_binary_steps; ++s)
        {
            const fdd mid = (r_in + r_out) * (fdd)0.5;
            if (inside(mid))
                r_in = mid;
            else
                r_out = mid;
        }

        const fdd r_est = r_in;
        if (r_est > best_r)
        {
            best_r = r_est;
            best_dir = dir;
        }
    }

    out_radius = best_r;
    out_cusp_like = center + complex_mul_scalar(best_dir, best_r);
}

struct CenterKey
{
    int64_t qx = 0;
    int64_t qy = 0;
    int period = 0;

    bool operator==(const CenterKey& o) const
    {
        return qx == o.qx && qy == o.qy && period == o.period;
    }
};

struct CenterKeyHash
{
    size_t operator()(const CenterKey& k) const
    {
        const uint64_t a = (uint64_t)k.qx;
        const uint64_t b = (uint64_t)k.qy;
        const uint64_t p = (uint64_t)(uint32_t)k.period;

        uint64_t h = a * 0x9E3779B185EBCA87ull;
        h ^= b + 0xC2B2AE3D27D4EB4Full + (h << 6) + (h >> 2);
        h ^= p + 0x165667B19E3779F9ull + (h << 6) + (h >> 2);
        return (size_t)h;
    }
};

static CenterKey make_center_key(const Complex<fdd>& c, int period, double cell)
{
    const double xr = (double)c.x;
    const double yi = (double)c.y;

    CenterKey k;
    k.qx = (int64_t)std::llround(xr / cell);
    k.qy = (int64_t)std::llround(yi / cell);
    k.period = period;
    return k;
}

static std::vector<MinibrotAtlasEntry> build_minibrot_atlas_by_zoom_f128(const MinibrotAtlasScanParams& scan)
{
    const MinibrotAtlasParams& params = scan.algo;

    std::vector<MinibrotAtlasEntry> out;
    out.reserve((size_t)std::min(scan.max_entries, 4096));

    std::unordered_set<CenterKey, CenterKeyHash> seen;
    seen.reserve((size_t)std::min(scan.max_entries * 2, 200000));

    const fdd main_center_to_cusp = fdd(0.25);
    const fdd r_min = main_center_to_cusp / scan.zoom_max;

    const double r_min_d = (double)r_min;
    const double finest_step = std::max(r_min_d * std::max(0.05, scan.step_factor), 1e-15);

    // _ChatGPT_ dedup cell is tied to the minimum size so tiny components do not merge
    const double dedup_cell = std::max(finest_step * 0.25, 1e-15);

    auto process_seed = [&](double re, double im)
    {
        if ((int)out.size() >= scan.max_entries)
            return;

        const Complex<fdd> c0((fdd)re, (fdd)im);

        const int p = detect_attracting_period(
            c0,
            params.max_period,
            params.detect_iter_max,
            params.detect_warmup,
            params.escape_r2,
            params.detect_eps2);

        if (p <= 0)
            return;

        if (!scan.include_main && p == 1)
            return;

        Complex<fdd> center;
        if (!compute_center_newton(c0, p, params.newton_max_iters, params.newton_tol2, params.newton_min_deriv2, center))
            return;

        const int p_min = minimal_period_at_center(center, p, params.newton_tol2);
        if (p_min != p)
            return;

        const CenterKey key = make_center_key(center, p, dedup_cell);
        if (seen.find(key) != seen.end())
            return;

        Complex<fdd> cusp_like((fdd)0, (fdd)0);
        fdd radius = (fdd)0;
        estimate_cusp_like_and_radius(center, p, params, cusp_like, radius);

        if (radius < r_min)
            return;

        const fdd zoom = main_center_to_cusp / radius;

        const Complex<double> dir_d = (Complex<double>)(cusp_like - center);
        const double rotation_rad = -std::atan2(dir_d.y, dir_d.x);

        MinibrotAtlasEntry e;
        e.center = center;
        e.cusp_like = cusp_like;
        e.zoom = zoom;
        e.rotation_rad = rotation_rad;
        e.period = p;
        e.est_radius = radius;
        blPrint() << "Found minibrot: center=" << (Complex<double>)center << " period=" << p << " zoom=" << (double)zoom << " radius=" << (double)radius;

        out.push_back(e);
        seen.insert(key);
    };

    auto scan_grid = [&](double step)
    {
        const double w = params.scan_max_re - params.scan_min_re;
        const double h = params.scan_max_im - params.scan_min_im;

        const int nx = (int)std::ceil(w / step);
        const int ny = (int)std::ceil(h / step);

        // _ChatGPT_ two-phase offset reduces aliasing that can miss narrow components between grid lines
        const double offsets[2] = { 0.0, 0.5 };

        for (double oy : offsets)
        {
            const double y0 = params.scan_min_im + oy * step;
            for (int iy = 0; iy <= ny; ++iy)
            {
                const double im = y0 + (double)iy * step;
                if (im < params.scan_min_im || im > params.scan_max_im)
                    continue;

                for (double ox : offsets)
                {
                    const double x0 = params.scan_min_re + ox * step;
                    for (int ix = 0; ix <= nx; ++ix)
                    {
                        const double re = x0 + (double)ix * step;
                        if (re < params.scan_min_re || re > params.scan_max_re)
                            continue;

                        process_seed(re, im);
                        if ((int)out.size() >= scan.max_entries)
                            return;
                    }
                }
            }
        }
    };

    if (!scan.progressive)
    {
        scan_grid(finest_step);
        return out;
    }

    double step = std::max(finest_step * 16.0, 0.2);
    while (step > finest_step * 1.0001)
    {
        scan_grid(step);
        if ((int)out.size() >= scan.max_entries)
            break;
        step *= 0.5;
    }

    if ((int)out.size() < scan.max_entries)
        scan_grid(finest_step);

    return out;
}
