#if MANDEL_UNVERSIONED_EXPERIMENTAL
#include "../config/build_config.h"
#include <bitloop.h>

#include "log_phi_coordinates.hpp"
#include "minibrot_atlas.hpp"

SIM_BEG;
using namespace bl;

void Mandel_Experimental::init()
{
    if (enabled_logphi)
        logphi_coords.init();

    if (enabled_minibrotAtlas)
    {
        auto results = build_baby_mandelbrot_atlas_f128_streaming(params);
    }
}


void Mandel_Experimental::process()
{
    if (enabled_logphi)
        logphi_coords.process();

    ChangeTracker::updateCurrent();
}

void Mandel_Experimental::draw(Viewport* ctx) const
{
    if (enabled_logphi)
        logphi_coords.draw(ctx);
}

/*void Mandelbrot_Scene::UI::populateExperimental()
{
    if (ImGui::Section("Experimental")) {
    {
        bl_scoped(input_angle, input_iters, input_quality);
        ImGui::SliderDouble("input_angle", &input_angle, 0.0, math::pi * 2.0);
        ImGui::SliderInt("input_iters", &input_iters, 1, 200);
        ImGui::SliderInt("input_quality", &input_quality, 1, 200);
    }
}

void Mandelbrot_Scene::UI::populateSplinesDev()
{
    bl_scoped(tween_pos_spline);
    bl_scoped(tween_zoom_lift_spline);
    bl_scoped(tween_base_zoom_spline);

    static ImRect vr = { 0.0f, 1.0f, 1.0f, 0.0f };

    ImGui::SeparatorText("Position Tween");
    ImSpline::SplineEditor("tween_pos", &tween_pos_spline, &vr);

    ImGui::SeparatorText("Lift Tween");
    ImSpline::SplineEditor("tween_zoom_lift", &tween_zoom_lift_spline, &vr);

    ImGui::SeparatorText("Base Zoom Tween");
    ImSpline::SplineEditor("tween_base_zoom", &tween_base_zoom_spline, &vr);

    //ImGui::InputTextMultiline("###pos_buf", pos_tween_buf, 1024, ImVec2(0, 0), ImGuiInputTextFlags_AllowTabInput));
    if (ImGui::Button("Copy position spline")) ImGui::SetClipboardText(tween_pos_spline.serialize(SplineSerializationMode::CPP_ARRAY, 3).c_str());

    //ImGui::InputTextMultiline("###pos_buf", zoom_tween_buf, 1024, ImVec2(0, 0), ImGuiInputTextFlags_AllowTabInput));
    if (ImGui::Button("Copy lift spline")) ImGui::SetClipboardText(tween_zoom_lift_spline.serialize(SplineSerializationMode::CPP_ARRAY, 3).c_str());

    if (ImGui::Button("Copy base zoom spline")) ImGui::SetClipboardText(tween_base_zoom_spline.serialize(SplineSerializationMode::CPP_ARRAY, 3).c_str());

    ///static std::string results;
    ///static std::string spline_results;
    ///
    ///bl_scoped(stripe_mag_numerator, stripe_mag_from_hist);
    ///ImGui::Checkbox("Stripe Use Histogram", &stripe_mag_from_hist);
    ///ImGui::SliderFloat("stripe_mag_numerator", &stripe_mag_numerator, 0.01f, 0.3f);
    ///
    ///bl_scoped(stripe_zf_spline, stripe_zf_spline_rect);
    ///bl_pull(ideal_zf_numerator_map);
    ///
    ///if (ImSpline::BeginSplineEditor("ZF Spline", &stripe_zf_spline, &stripe_zf_spline_rect, 300.0f, ImSplineFlags_InvertY))
    ///{
    ///    if (ideal_zf_numerator_map.size()) {
    ///        for (const auto& [key, value] : ideal_zf_numerator_map)
    ///            ImSpline::PlotPoint(key, value);
    ///    }
    ///    ImSpline::EndSplineEditor();
    ///}
    ///
    ///if (ImGui::Button("Update Results"))
    ///{
    ///    std::stringstream ss;
    ///    for (const auto& [key, value] : ideal_zf_numerator_map)
    ///        ss << key << " = " << value << "\n";
    ///
    ///    results = ss.str();
    ///    spline_results = stripe_zf_spline.serialize(SplineSerializationMode::CPP_ARRAY, 3);
    ///}
    ///ImGui::InputTextMultiline("Results", &results);
    ///ImGui::InputTextMultiline("Spline", &spline_results);
}*/


SIM_END;
#endif