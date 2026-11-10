#include <bitloop/util/math_util.h>

#include "../Mandelbrot.h"
#include "conversions.h"

SIM_BEG;

void Mandelbrot_Scene::beginSteadyZoom()
{
    std::string data = serializeState();

    captureContext().setFixedFrameTimeDelta(true);

    steady_zoom_frames_elapsed = 0;

    // Give destination same reference zoom level
    state_b.camera.setReferenceZoom(camera.getReferenceZoom());

    // Set destination
    state_b.deserialize(data);

    // Set current state to match, but reset back to current zoom
    f64 current_zoom = camera.relativeZoom();
    static_cast<MandelState&>(*this) = state_b;
    camera.setRelativeZoom(current_zoom);

    // Lock stripe mean on target
    stripe_mean_locked = activeField().raw_mean_stripe;

    // Mark starting state for checking lerp progress
    state_a = static_cast<MandelState&>(*this);

    // Begin tweening
    tweening = true;
    steady_zoom = true;

    if (record_steady_zoom)
    {
        beginRecording();
    }
}

void Mandelbrot_Scene::endSteadyZoom()
{
    camera.setRelativeZoom(state_b.camera.relativeZoom());
    tweening = false;
    steady_zoom = false;

    endRecording();
}

void Mandelbrot_Scene::updateSteadyZoom(double dt)
{
    if (tweening)
    {
        if (steady_zoom)
        {
            bool finished_frame = capturedLastFrame();
            if (finished_frame)
            {
                if (camera.relativeZoom() < state_b.camera.relativeZoom())
                {
                    auto stepsToReach = [](f64 A, f64 B, f64 zoom_rate) {
                        double n = (double)(bl::log(B / A) / bl::log(zoom_rate));
                        return (int)bl::ceil(n);
                    };

                    steady_zoom_frames_elapsed++;

                    steady_zoom_expected_frames = stepsToReach(
                        state_a.camera.relativeZoom(),
                        state_b.camera.relativeZoom(),
                        1.0 + steady_zoom_mult_speed); // seconds

                    camera.setRelativeZoom(camera.relativeZoom() * (1.0 + steady_zoom_mult_speed));
                    camera.setRotation(camera.rotation() + steady_zoom_rotate_speed);

                    steady_zoom_pct = (float)math::lerpFactor(
                        toNormalizedZoom(camera.relativeZoom()),
                        toNormalizedZoom(state_a.camera.relativeZoom()),
                        toNormalizedZoom(state_b.camera.relativeZoom())
                    ) * 100.0f;

                    // Update estimated time remaining
                    double expected_time_left = dt * (steady_zoom_expected_frames - steady_zoom_frames_elapsed);
                    expected_time_left_ma.push(expected_time_left);
                }
                else
                {
                    endSteadyZoom();
                }
            }
        }
    }
}



SIM_END;
