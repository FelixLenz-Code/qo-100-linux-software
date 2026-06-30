// QO-100 waterfall viewer — loads a .cf32 capture, shows spectrum + waterfall,
// lets you tune by dragging a marker, and decodes the selected USB signal to a
// WAV file. No audio device needed, so it is usable on a display-only machine.
//
//   qo100_ui [capture.cf32]

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include "../engine/calib.h"
#include "../engine/filedevice.h"
#include "../engine/iqfile.h"
#include "../engine/qo100.h"
#include "../engine/rx.h"
#include "../engine/spectrum.h"
#include "../engine/stream.h"
#include "../engine/wavfile.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace qo100;

namespace {

ImFont* g_fontBody = nullptr;
ImFont* g_fontHeader = nullptr;
ImFont* g_fontSmall = nullptr;

ImVec4 rgb(int hex, float a = 1.0f) {
    return ImVec4(((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f,
                  (hex & 0xFF) / 255.0f, a);
}

const ImVec4 kAccent = rgb(0x2DD4BF);     // teal
const ImVec4 kAccentHi = rgb(0x5EEAD4);
const ImVec4 kAccentLo = rgb(0x0F766E);
const ImVec4 kMuted = rgb(0x7C8493);

struct App {
    std::string path = "scene.cf32";
    std::vector<cf32> iq;
    bool haveData = false;
    std::string status = "Aufnahme wählen und »Laden« drücken.";

    float fsIn = 384000.0f;
    int decim = 8;
    double tune = 50000.0;
    float dbMin = -120.0f, dbMax = -10.0f;

    // QO-100 frequency context.
    double centerDownlinkMHz = 10489.750; // RF downlink at the centre of the capture
    double beaconOffset = 20000.0;        // where a known beacon sits in the capture
    double calHz = 0.0;                   // LNB drift from the last calibration
    double calSnr = 0.0;

    int fftSize = 1024;
    int rows = 400;
    std::vector<float> waterfall;
    std::vector<float> avgSpec;
    std::vector<float> freqs;

    // Live streaming.
    bool live = false;
    std::unique_ptr<FileDevice> dev;
    std::unique_ptr<StreamEngine> eng;

    void resetDisplay() {
        waterfall.assign((size_t)rows * fftSize, dbMin);
        avgSpec.assign(fftSize, dbMin);
        freqs.resize(fftSize);
        for (int b = 0; b < fftSize; ++b) freqs[b] = (b - fftSize / 2) * fsIn / fftSize;
    }

    void startLive() {
        stopLive();
        dev = std::make_unique<FileDevice>(path, fsIn, /*realtime=*/true);
        if (!dev->start()) {
            status = "Live: »" + path + "« nicht lesbar";
            dev.reset();
            return;
        }
        fftSize = 1024;
        resetDisplay();
        haveData = true;
        eng = std::make_unique<StreamEngine>(*dev, decim, fftSize);
        eng->setTune(tune);
        eng->start();
        live = true;
        status = "Live läuft …";
    }

    void stopLive() {
        if (eng) { eng->stop(); eng.reset(); }
        if (dev) { dev->stop(); dev.reset(); }
        if (live) status = "Live gestoppt";
        live = false;
    }

    void pollLive() {
        if (!live || !eng) return;
        eng->setTune(tune);
        std::vector<float> row;
        if (eng->latestSpectrum(row) && (int)row.size() == fftSize) {
            // Scroll down: newest row on top.
            std::memmove(waterfall.data() + fftSize, waterfall.data(),
                         (size_t)(rows - 1) * fftSize * sizeof(float));
            std::copy(row.begin(), row.end(), waterfall.begin());
            avgSpec = row;
        }
    }

    void load() {
        iq.clear();
        if (!iqfile::read(path, iq)) {
            status = "Fehler: »" + path + "« nicht lesbar";
            haveData = false;
            return;
        }
        computeWaterfall();
        haveData = true;
        status = "Geladen: " + std::to_string(iq.size()) + " IQ-Samples";
    }

    void computeWaterfall() {
        Spectrum spec(fftSize);
        waterfall.assign((size_t)rows * fftSize, dbMin);
        avgSpec.assign(fftSize, 0.0f);
        freqs.resize(fftSize);
        for (int b = 0; b < fftSize; ++b) freqs[b] = (b - fftSize / 2) * fsIn / fftSize;
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

    // Real downlink/uplink frequency for a given baseband offset, drift-corrected.
    double downlinkHz(double offset) const {
        return centerDownlinkMHz * 1e6 + offset - calHz;
    }
    double uplinkHz(double offset) const { return plan::uplinkForDownlink(downlinkHz(offset)); }

    void calibrate() {
        if (!haveData) { status = "Erst eine Aufnahme laden."; return; }
        BeaconCalibrator cal(fsIn);
        const CalResult r = cal.find(iq, beaconOffset, 8000.0);
        if (!r.found) {
            status = "Kein Beacon nahe " + std::to_string((long)beaconOffset) +
                     " Hz (SNR " + std::to_string((int)r.snrDb) + " dB)";
            return;
        }
        calHz = r.errorHz;
        calSnr = r.snrDb;
        char b[96];
        std::snprintf(b, sizeof(b), "Kalibriert: LNB-Drift %.1f Hz  (SNR %.0f dB)", calHz, calSnr);
        status = b;
    }

    void saveConfig() const {
        std::ofstream f("qo100.cfg");
        if (!f) return;
        f << "path=" << path << "\n"
          << "fsIn=" << fsIn << "\n"
          << "decim=" << decim << "\n"
          << "tune=" << tune << "\n"
          << "dbMin=" << dbMin << "\n"
          << "dbMax=" << dbMax << "\n"
          << "centerDownlinkMHz=" << centerDownlinkMHz << "\n"
          << "beaconOffset=" << beaconOffset << "\n"
          << "calHz=" << calHz << "\n";
    }

    void loadConfig() {
        std::ifstream f("qo100.cfg");
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            if (k == "path") path = v;
            else if (k == "fsIn") fsIn = std::stof(v);
            else if (k == "decim") decim = std::stoi(v);
            else if (k == "tune") tune = std::stod(v);
            else if (k == "dbMin") dbMin = std::stof(v);
            else if (k == "dbMax") dbMax = std::stof(v);
            else if (k == "centerDownlinkMHz") centerDownlinkMHz = std::stod(v);
            else if (k == "beaconOffset") beaconOffset = std::stod(v);
            else if (k == "calHz") calHz = std::stod(v);
        }
    }

    void decode() {
        if (!haveData) { status = "Erst eine Aufnahme laden."; return; }
        RxChain rx(fsIn, decim);
        rx.setTune(tune);
        std::vector<float> audio;
        rx.process(iq, audio);
        const int rate = (int)rx.audioRate();
        if (!wavfile::writeMono("decoded.wav", audio, rate)) {
            status = "Fehler beim Schreiben von decoded.wav";
            return;
        }
        status = "Dekodiert @ " + std::to_string(rate) + " Hz  »  decoded.wav";
    }
};

void sectionHeader(const char* text) {
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushFont(g_fontSmall);
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Spacing();
}

void labeledInputFloat(const char* label, const char* id, float* v, const char* fmt) {
    ImGui::TextColored(kMuted, "%s", label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputFloat(id, v, 0, 0, fmt);
}

void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.ChildRounding = 12.0f;
    s.FrameRounding = 8.0f;
    s.GrabRounding = 8.0f;
    s.PopupRounding = 8.0f;
    s.ScrollbarRounding = 10.0f;
    s.TabRounding = 8.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = ImVec2(22, 20);
    s.FramePadding = ImVec2(12, 9);
    s.ItemSpacing = ImVec2(12, 11);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 12.0f;
    s.GrabMinSize = 12.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = rgb(0xE6E9F0);
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_WindowBg] = rgb(0x0F1117);
    c[ImGuiCol_ChildBg] = rgb(0x171A22);
    c[ImGuiCol_PopupBg] = rgb(0x171A22);
    c[ImGuiCol_Border] = rgb(0x262B36);
    c[ImGuiCol_FrameBg] = rgb(0x21262F);
    c[ImGuiCol_FrameBgHovered] = rgb(0x2B313D);
    c[ImGuiCol_FrameBgActive] = rgb(0x333A48);
    c[ImGuiCol_Button] = rgb(0x21262F);
    c[ImGuiCol_ButtonHovered] = rgb(0x2B313D);
    c[ImGuiCol_ButtonActive] = rgb(0x333A48);
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = kAccentHi;
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_Header] = rgb(0x21262F);
    c[ImGuiCol_HeaderHovered] = rgb(0x2B313D);
    c[ImGuiCol_HeaderActive] = rgb(0x333A48);
    c[ImGuiCol_Separator] = rgb(0x262B36);
    c[ImGuiCol_SeparatorHovered] = kAccentLo;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = rgb(0x2B313D);
    c[ImGuiCol_ScrollbarGrabHovered] = rgb(0x333A48);
    c[ImGuiCol_FrameBgActive] = rgb(0x333A48);

    ImPlotStyle& p = ImPlot::GetStyle();
    p.PlotPadding = ImVec2(14, 12);
    p.LabelPadding = ImVec2(6, 6);
    p.PlotBorderSize = 0.0f;
    p.MajorGridSize = ImVec2(1, 1);
    p.Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
    p.Colors[ImPlotCol_PlotBg] = rgb(0x0F1117);
    p.Colors[ImPlotCol_PlotBorder] = ImVec4(0, 0, 0, 0);
    p.Colors[ImPlotCol_AxisGrid] = rgb(0x262B36, 0.5f);
    p.Colors[ImPlotCol_AxisText] = kMuted;
    p.Colors[ImPlotCol_LegendBg] = rgb(0x171A22, 0.9f);
    p.Colors[ImPlotCol_LegendText] = rgb(0xE6E9F0);
}

void loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    auto tryFont = [&](const char* path, float size) -> ImFont* {
        std::ifstream f(path);
        if (!f.good()) return nullptr;
        return io.Fonts->AddFontFromFileTTF(path, size);
    };
    const char* reg = "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf";
    const char* bold = "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf";
    const char* dejavu = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

    g_fontBody = tryFont(reg, 18.0f);
    if (!g_fontBody) g_fontBody = tryFont(dejavu, 17.0f);
    if (!g_fontBody) g_fontBody = io.Fonts->AddFontDefault();

    g_fontHeader = tryFont(bold, 30.0f);
    if (!g_fontHeader) g_fontHeader = g_fontBody;

    g_fontSmall = tryFont(bold, 13.0f);
    if (!g_fontSmall) g_fontSmall = g_fontBody;
}

void drawSidebar(App& app) {
    ImGui::BeginChild("sidebar", ImVec2(360, 0),
                      ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding);

    sectionHeader("QUELLE");
    ImGui::TextColored(kMuted, ".cf32-Aufnahme");
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", app.path.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##path", buf, sizeof(buf))) app.path = buf;
    const float halfW =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Laden", ImVec2(halfW, 0))) { app.stopLive(); app.load(); }
    ImGui::SameLine();
    if (!app.live) {
        ImGui::PushStyleColor(ImGuiCol_Button, kAccentLo);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
        if (ImGui::Button("Live", ImVec2(halfW, 0))) app.startLive();
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, rgb(0x9F1239));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(0xBE123C));
        if (ImGui::Button("Stop", ImVec2(halfW, 0))) app.stopLive();
        ImGui::PopStyleColor(2);
    }

    sectionHeader("EMPFANG");
    labeledInputFloat("Abtastrate  [Hz]", "##fs", &app.fsIn, "%.0f");
    ImGui::TextColored(kMuted, "Dezimierung");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputInt("##decim", &app.decim) && app.decim < 1) app.decim = 1;
    ImGui::TextColored(kMuted, "Tune-Offset  [Hz]");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputDouble("##tune", &app.tune, 100.0, 1000.0, "%.0f");

    sectionHeader("FREQUENZ");
    ImGui::TextColored(kMuted, "Downlink-Mitte  [MHz]");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputDouble("##cdl", &app.centerDownlinkMHz, 0.001, 0.01, "%.3f");
    {
        char dl[48], ul[48];
        std::snprintf(dl, sizeof(dl), "%.4f MHz", app.downlinkHz(app.tune) / 1e6);
        std::snprintf(ul, sizeof(ul), "%.4f MHz", app.uplinkHz(app.tune) / 1e6);
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::TextColored(kMuted, "Downlink (RX)");
        ImGui::PushFont(g_fontHeader);
        ImGui::PushStyleColor(ImGuiCol_Text, kAccentHi);
        ImGui::TextUnformatted(dl);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::TextColored(kMuted, "Uplink (TX)");
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(0xFBBF24));
        ImGui::TextUnformatted(ul);
        ImGui::PopStyleColor();
    }

    sectionHeader("KALIBRIERUNG");
    ImGui::TextColored(kMuted, "Beacon-Offset  [Hz]");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputDouble("##beac", &app.beaconOffset, 100.0, 1000.0, "%.0f");
    if (ImGui::Button("Auf Beacon kalibrieren", ImVec2(-FLT_MIN, 0))) app.calibrate();
    {
        char c[64];
        std::snprintf(c, sizeof(c), "LNB-Drift: %.1f Hz", app.calHz);
        ImGui::TextColored(kMuted, "%s", c);
    }

    sectionHeader("ANZEIGE");
    ImGui::TextColored(kMuted, "Pegelbereich  [dBFS]");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##dbmin", &app.dbMin, -160.0f, 0.0f, "min %.0f");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##dbmax", &app.dbMax, -160.0f, 20.0f, "max %.0f");

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::PushStyleColor(ImGuiCol_Button, kAccentLo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentHi);
    if (ImGui::Button("Dekodieren  »  decoded.wav", ImVec2(-FLT_MIN, 44))) app.decode();
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped("%s", app.status.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 4));
    if (ImGui::SmallButton("Einstellungen sichern")) {
        app.saveConfig();
        app.status = "Einstellungen in qo100.cfg gesichert";
    }

    ImGui::EndChild();
}

void drawContent(App& app) {
    ImGui::BeginChild("content", ImVec2(0, 0));

    if (!app.haveData) {
        ImGui::Dummy(ImVec2(0, 40));
        ImGui::PushFont(g_fontHeader);
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        const char* msg = "Keine Aufnahme geladen";
        const float w = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - w) * 0.5f);
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::EndChild();
        return;
    }

    if (app.live) {
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(0x34D399));
        ImGui::TextUnformatted("LIVE");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(kMuted, "S-Meter");
        ImGui::SameLine();
        const double lvl = app.eng ? app.eng->audioLevel() : 0.0;
        const float meter = (float)std::min(1.0, lvl / 0.5);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, kAccent);
        ImGui::ProgressBar(meter, ImVec2(-FLT_MIN, 14), "");
        ImGui::PopStyleColor();
    }

    const double halfBand = app.fsIn / 2.0;
    const float avail = ImGui::GetContentRegionAvail().y;
    const float wfH = avail * 0.62f - 6.0f;

    ImPlot::PushColormap(ImPlotColormap_Viridis);
    if (ImPlot::BeginPlot("##wf", ImVec2(-1, wfH),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Frequenz  [Hz, relativ zur Bandmitte]", "Zeit",
                          0, ImPlotAxisFlags_NoTickLabels);
        ImPlot::SetupAxisLimits(ImAxis_X1, -halfBand, halfBand, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, app.rows, ImPlotCond_Once);
        ImPlot::PlotHeatmap("##h", app.waterfall.data(), app.rows, app.fftSize,
                            app.dbMin, app.dbMax, nullptr,
                            ImPlotPoint(-halfBand, 0), ImPlotPoint(halfBand, app.rows));
        ImPlot::SetNextLineStyle(rgb(0xFBBF24), 2.0f); // amber tune marker
        ImPlot::DragLineX(0, &app.tune, rgb(0xFBBF24), 2.0f);
        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();

    if (ImPlot::BeginPlot("##spec", ImVec2(-1, -1),
                          ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Frequenz  [Hz, relativ zur Bandmitte]", "dBFS");
        ImPlot::SetupAxisLimits(ImAxis_X1, -halfBand, halfBand, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, app.dbMin, app.dbMax + 5.0, ImPlotCond_Always);
        ImPlot::SetNextFillStyle(kAccent, 0.18f);
        ImPlot::PlotShaded("##fill", app.freqs.data(), app.avgSpec.data(), app.fftSize, -300.0);
        ImPlot::SetNextLineStyle(kAccent, 2.0f);
        ImPlot::PlotLine("##line", app.freqs.data(), app.avgSpec.data(), app.fftSize);
        ImPlot::DragLineX(0, &app.tune, rgb(0xFBBF24), 2.0f);
        ImPlot::EndPlot();
    }

    ImGui::EndChild();
}

void drawUi(App& app) {
    app.pollLive();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    // Header.
    ImGui::PushFont(g_fontHeader);
    ImGui::TextColored(kAccent, "QO-100");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::Text("  Linux SSB · Wasserfall & Decoder");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 6));

    drawSidebar(app);
    ImGui::SameLine();
    drawContent(app);

    ImGui::End();
}

} // namespace

int main(int argc, char** argv) {
    App app;
    app.loadConfig();              // restore last session if present
    bool wantLive = false;         // --live starts streaming immediately
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--live") wantLive = true;
        else app.path = a; // explicit file overrides the saved path
    }

    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1320, 860, "QO-100 Linux", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "window creation failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    loadFonts();
    applyTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    if (wantLive) app.startLive();
    else app.load(); // restored or supplied path; harmlessly reports if missing

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::PushFont(g_fontBody);

        drawUi(app);

        ImGui::PopFont();
        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    app.stopLive();   // tear down streaming threads cleanly
    app.saveConfig(); // remember this session

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
