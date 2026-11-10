#pragma once
#include <bitloop.h>

#include <cmath>
#include <vector>

// don't use FloatType::F32 - remap from F64 (F32 offers no advantage to performance and bloats dispatch tables)
enum struct MandelFloatType { F64, F128, F256, COUNT };
bl_enable_enum_bitops(MandelFloatType);
bl_map_enum_to_type(MandelFloatType::F64,  bl::f64);
bl_map_enum_to_type(MandelFloatType::F128, bl::fdd);
bl_map_enum_to_type(MandelFloatType::F256, bl::fqd);

enum class KernelFeatures
{
    NONE    = 0b000,
    ITER    = 0b001,
    DIST    = 0b010,
    STRIPES = 0b100,
    MIX_ALL = 0b111,

    COUNT
};

template<>
struct bl_enum_domain_traits<KernelFeatures>
{
    static constexpr std::size_t size = static_cast<std::size_t>(KernelFeatures::MIX_ALL);
    static constexpr std::size_t index(KernelFeatures v) {
        const auto u = static_cast<std::size_t>(v);
        assert(u >= 1 && u <= size);
        return u - 1;
    }

    static constexpr KernelFeatures value(std::size_t i) {
        return static_cast<KernelFeatures>(i + 1);
    }
};

enum struct SplitScreenMode : uint32_t
{
    SINGLE_VIEW,
    SPLIT_VIEW
};

enum struct FractalFormula : uint32_t
{
    MANDELBROT,

    COUNT
};

enum struct ComputeBackend : uint32_t
{
    DIRECT,

    // perturbation
    PERTURBATION,
    //PERTURBATION_SIMD, // kept for development / easier debugging
    PERTURBATION_SIMD_UNROLLED,

    COUNT,
    AUTO = COUNT, // Same value as COUNT to avoid being included in constexpr_dispatch 'build_table'
};

static inline const char* FractalFormulaNames[(int)FractalFormula::COUNT] =
{
    "MANDELBROT"
};

static inline const char* ComputeBackendNames[(int)ComputeBackend::COUNT] =
{
    "DIRECT",
    "PERTURBATION",
    //"PERTURBATION_SIMD",
    "PERTURBATION_SIMD_UNROLLED"
};

// todo: find way to put inside SIM_BEG ns (wasm32 error, must be in global)
bl_enable_enum_bitops(SplitScreenMode);
bl_enable_enum_bitops(KernelFeatures);

SIM_BEG;

using namespace bl;

//constexpr double INSIDE_MANDELBROT_SET = std::numeric_limits<double>::max();
constexpr double INSIDE_MANDELBROT_SET = std::numeric_limits<float>::max();
const double INSIDE_MANDELBROT_SET_SKIPPED = std::nextafter(INSIDE_MANDELBROT_SET, 0.0f);

enum MandelFlag : uint32_t
{
    // bits
    MANDEL_DYNAMIC_ITERS        = 1u << 0,
    MANDEL_SHOW_AXIS            = 1u << 1,
    //MANDEL_FLATTEN              = 1u << 2,
    MANDEL_DYNAMIC_COLOR_CYCLE  = 1u << 3,
    MANDEL_NORMALIZE_DEPTH      = 1u << 4,
    MANDEL_INVERT_DIST          = 1u << 5,
    MANDEL_USE_SMOOTHING        = 1u << 6,

    // bitmasks
    //MANDEL_FLAGS_MASK   = 0x000FFFFFu, // max 24 bit-flags
    //MANDEL_SMOOTH_MASK  = 0x00F00000u, // max 16 smooth types
    MANDEL_VERSION_MASK   = 0xFF000000u, // max 255 versions

    MANDEL_VERSION_BITSHIFT = 24
};

enum MandelPhase : int
{
    PHASE_27X,
    PHASE_9X,
    PHASE_3X,
    PHASE_1X,
    //
    PHASE_COUNT
};

enum struct MandelInteriorForwarding
{
    SLOWEST,
    SLOW,
    MEDIUM,
    FAST,
    COUNT
};

static inline const char* MandelMaxDepthOptimizationNames[(int)MandelInteriorForwarding::COUNT] =
{
    "Slowest (Lossless)",
    "Slow",
    "Balanced",
    "Fast (Loss at sharp angles)"
};

static inline const char* FloatTypeNames[3] =
{
    //"32-bit",
    "64-bit",
    "128-bit",
    "256-bit"
};


SIM_END;
