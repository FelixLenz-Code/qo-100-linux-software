// QO-100 waterfall viewer — loads a .cf32 capture, shows spectrum + waterfall,
// lets you tune by dragging a marker, and decodes the selected USB signal to a
// WAV file. No audio device needed, so it is usable on a display-only machine.
//
//   qo100_ui [capture.cf32]

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include "../engine/fft.h"
#include "../engine/iqfile.h"
#include "../engine/rx.h"
#include "../engine/spectrum.h"
#include "../engine/wavfile.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace qo100;

namespace {

struct App {
    std::string path = "scene.cf32";
    std::vector<cf32> iq;
    bool haveData = false;
    std::string status = "Datei wählen und 'Laden' drücken.";

    // Parameters.
    float fsIn = 384000.0f;
    int decim = 8;
    double tune = 50000.0;
    float dbMin = -120.0f, dbMax = -10.0f;

    // Waterfall / spectrum data.
    int fftSize = 1024;
    int rows = 400;
    std::vector<float> waterfall; // rows * fftSize, row 0 = start of capture
    std::vector<float> avgSpec;   // fftSize
    std::vector<float> freqs;     // fftSize, Hz relative to band centre

    void load() {
        iq.clear();
        if (!iqfile::read(path, iq)) {
            status = "Fehler: kann '" + path + "' nicht lesen";
            haveData = false;
            return;
        }
        computeWaterfall();
        haveData = true;
        status = "geladen: " + std::to_string(iq.size()) + " IQ-Samples";
    }

    void computeWaterfall() {
        Spectrum spec(fftSize);
        waterfall.assign((size_t)rows * fftSize, dbMin);
        avgSpec.assign(fftSize, 0.0f);
        freqs.resize(fftSize);
        for (int b = 0; b < fftSize; ++b)
            freqs[b] = (b - fftSize / 2) * fsIn / fftSize;
        const long total = (long)iq.size();
        if (total < fftSize) return;
        const long hop = std::max<long>(1, (total - fftSize) / std::max(1, rows - 1));
        std::vector<float> row(fftSize);
        for (int r = 0; r < rows; ++r) {
            long start = (long)r * hop;
            if (start + fftSize > total) start = total - fftSize;
            spec.compute(&iq[start], row);
            for (int b = 0; b < fftSize; ++b) {
                waterfall[(size_t)r * fftSize + b] = row[b];
                avgSpec[b] += row[b];
            }
        }
        for (auto& v : avgSpec) v /= rows;
    }

    void decode() {
        if (!haveData) { status = "Erst eine Datei laden."; return; }
        RxChain rx(fsIn, decim);
        rx.setTune(tune);
        std::vector<float> audio;
        rx.process(iq, audio);
        const int rate = (int)rx.audioRate();
        if (!wavfile::writeMono("decoded.wav", audio, rate)) {
            status = "Fehler beim Schreiben von decoded.wav";
            return;
        }
        status = "dekodiert @ " + std::to_string(rate) + " Hz -> decoded.wav (" +
                 std::to_string(audio.size()) + " Samples)";
    }
};

void drawUi(App& app) {
    ImGui::Begin("QO-100");

    ImGui::TextUnformatted(".cf32-Aufnahme:");
    ImGui::SameLine();
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", app.path.c_str());
    if (ImGui::InputText("##path", buf, sizeof(buf))) app.path = buf;
    ImGui::SameLine();
    if (ImGui::Button("Laden")) app.load();

    ImGui::InputFloat("Abtastrate fsIn [Hz]", &app.fsIn, 0, 0, "%.0f");
    ImGui::InputInt("Dezimierung", &app.decim);
    if (app.decim < 1) app.decim = 1;
    double tuneTmp = app.tune;
    if (ImGui::InputDouble("Tune-Offset [Hz]", &tuneTmp, 100.0, 1000.0, "%.0f")) app.tune = tuneTmp;
    ImGui::SliderFloat("dB min", &app.dbMin, -160.0f, 0.0f, "%.0f");
    ImGui::SliderFloat("dB max", &app.dbMax, -160.0f, 20.0f, "%.0f");

    if (ImGui::Button("Dekodieren -> decoded.wav")) app.decode();
    ImGui::SameLine();
    ImGui::TextUnformatted(app.status.c_str());

    if (app.haveData) {
        const double halfBand = app.fsIn / 2.0;

        if (ImPlot::BeginPlot("Wasserfall", ImVec2(-1, 320))) {
            ImPlot::SetupAxes("Hz (relativ zur Bandmitte)", "Zeit (Frame)");
            ImPlot::SetupAxisLimits(ImAxis_X1, -halfBand, halfBand, ImPlotCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, app.rows, ImPlotCond_Once);
            ImPlot::PlotHeatmap("##wf", app.waterfall.data(), app.rows, app.fftSize,
                                app.dbMin, app.dbMax, nullptr,
                                ImPlotPoint(-halfBand, 0), ImPlotPoint(halfBand, app.rows));
            ImPlot::DragLineX(0, &app.tune, ImVec4(1, 1, 0, 1));
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("Spektrum (Mittelwert)", ImVec2(-1, 200))) {
            ImPlot::SetupAxes("Hz (relativ zur Bandmitte)", "dBFS");
            ImPlot::SetupAxisLimits(ImAxis_X1, -halfBand, halfBand, ImPlotCond_Once);
            ImPlot::PlotLine("avg", app.freqs.data(), app.avgSpec.data(), app.fftSize);
            ImPlot::DragLineX(0, &app.tune, ImVec4(1, 1, 0, 1));
            ImPlot::EndPlot();
        }
    }

    ImGui::End();
}

} // namespace

int main(int argc, char** argv) {
    App app;
    if (argc > 1) { app.path = argv[1]; app.load(); }

    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1100, 800, "QO-100 Linux", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "window creation failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawUi(app);

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
