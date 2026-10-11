// Standalone D3D11 WARP oracle for the actual embedded AO shaders.
// Build: cl /EHsc /std:c++17 ao_warp_test.cpp d3d11.lib d3dcompiler.lib
// Run: ao_warp_test.exe <absolute path to kelvin_gpu_post.inc>
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;
static constexpr UINT W = 96, H = 72;
static void require(bool yes, const char *message) { if (!yes) throw std::runtime_error(message); }
static void hr(HRESULT result, const char *message) { require(SUCCEEDED(result), message); }
static std::string source_hlsl(const char *path) {
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot read renderer source");
    std::string input((std::istreambuf_iterator<char>(file)), {});
    size_t start = input.find("static const char kgpu_hlsl_post[] =");
    require(start != std::string::npos, "Cannot locate embedded shaders");
    start = input.find('=', start) + 1;
    std::string output;
    bool quoted = false;
    for (size_t i = start; i < input.size(); ++i) {
        char c = input[i];
        if (!quoted) { if (c == ';') return output; if (c == '"') quoted = true; continue; }
        if (c == '"') { quoted = false; continue; }
        if (c == '\\') {
            require(++i < input.size(), "Invalid shader string escape");
            c = input[i];
            if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
        }
        output += c;
    }
    throw std::runtime_error("Unterminated embedded shader string");
}
static ComPtr<ID3DBlob> compile(const std::string &hlsl, const char *entry, const char *profile) {
    ComPtr<ID3DBlob> blob, errors;
    HRESULT result = D3DCompile(hlsl.data(), hlsl.size(), "kelvin_gpu_post.inc", nullptr, nullptr, entry, profile,
                               D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &errors);
    if (FAILED(result)) {
        if (errors) std::cerr << static_cast<const char *>(errors->GetBufferPointer());
        throw std::runtime_error(std::string("Shader failed: ") + entry);
    }
    return blob;
}
struct Tex {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11RenderTargetView> rtv;
};
struct Cb { float p0[4]{}, p1[4]{}, p2[4]{}, p3[4]{}; };
struct Oracle {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11Buffer> cb;
    std::array<ComPtr<ID3D11PixelShader>, 4> methods;
    ComPtr<ID3D11PixelShader> blur_h, blur_v, composite;
    ComPtr<ID3D11SamplerState> point, linear;
    explicit Oracle(const std::string &hlsl) {
        hr(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                            &device, nullptr, &ctx), "Cannot create WARP device");
        const char *vs_src = "float4 main(uint id:SV_VertexID):SV_Position { float2 uv=float2((id<<1)&2,id&2);"
                             "return float4(uv*float2(2,-2)+float2(-1,1),0,1); }";
        auto vb = compile(vs_src, "main", "vs_5_0");
        hr(device->CreateVertexShader(vb->GetBufferPointer(), vb->GetBufferSize(), nullptr, &vs), "Create VS failed");
        const char *entries[] = { "ao_ssao_ps", "ao_hbao_ps", "ao_hbaop_ps", "ao_gtao_ps" };
        for (int i = 0; i < 4; ++i) methods[i] = ps(hlsl, entries[i]);
        blur_h = ps(hlsl, "ao_blur_h_ps"); blur_v = ps(hlsl, "ao_blur_v_ps"); composite = ps(hlsl, "ao_composite_ps");
        // Compile all existing effects too: adding AO must keep the old source valid.
        const char *existing[] = { "focus_ps", "coc_ps", "dof_ps", "fxaa_ps", "sharpen_ps", "ssaa_ps" };
        for (const char *entry : existing) compile(hlsl, entry, "ps_5_0");
        D3D11_BUFFER_DESC bd{}; bd.ByteWidth = sizeof(Cb); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        hr(device->CreateBuffer(&bd, nullptr, &cb), "Create CB failed");
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX; sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        hr(device->CreateSamplerState(&sd, &point), "Create point sampler failed");
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        hr(device->CreateSamplerState(&sd, &linear), "Create linear sampler failed");
    }
    ComPtr<ID3D11PixelShader> ps(const std::string &hlsl, const char *entry) {
        auto blob = compile(hlsl, entry, "ps_5_0");
        ComPtr<ID3D11PixelShader> shader;
        hr(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader), "Create PS failed");
        return shader;
    }
    Tex texture(DXGI_FORMAT format, const void *data = nullptr, bool target = true, UINT width = W, UINT height = H) {
        Tex tex;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = width; td.Height = height; td.MipLevels = td.ArraySize = 1; td.Format = format; td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | (target ? D3D11_BIND_RENDER_TARGET : 0);
        D3D11_SUBRESOURCE_DATA initial{}; initial.pSysMem = data; initial.SysMemPitch = width * (format == DXGI_FORMAT_R8_UNORM ? 1 : 4);
        hr(device->CreateTexture2D(&td, data ? &initial : nullptr, &tex.texture), "Create texture failed");
        hr(device->CreateShaderResourceView(tex.texture.Get(), nullptr, &tex.srv), "Create SRV failed");
        if (target) hr(device->CreateRenderTargetView(tex.texture.Get(), nullptr, &tex.rtv), "Create RTV failed");
        return tex;
    }
    void pass(ID3D11PixelShader *shader, Tex &out, ID3D11ShaderResourceView *t0, ID3D11ShaderResourceView *t1,
              ID3D11ShaderResourceView *t2, const Cb &constants, ID3D11ShaderResourceView *t3 = nullptr) {
        ID3D11RenderTargetView *rtv = out.rtv.Get();
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_TEXTURE2D_DESC desc{}; out.texture->GetDesc(&desc);
        D3D11_VIEWPORT vp{}; vp.Width = float(desc.Width); vp.Height = float(desc.Height); vp.MaxDepth = 1;
        ctx->RSSetViewports(1, &vp);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs.Get(), nullptr, 0); ctx->PSSetShader(shader, nullptr, 0);
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &constants, 0, 0);
        ID3D11Buffer *buffer = cb.Get(); ctx->PSSetConstantBuffers(0, 1, &buffer);
        ID3D11SamplerState *samplers[] = { linear.Get(), point.Get() }; ctx->PSSetSamplers(0, 2, samplers);
        ID3D11ShaderResourceView *inputs[] = { t0, t1, t2, t3 }; ctx->PSSetShaderResources(0, 4, inputs);
        ctx->Draw(3, 0);
        ID3D11ShaderResourceView *none[4]{}; ctx->PSSetShaderResources(0, 4, none);
        ctx->OMSetRenderTargets(0, nullptr, nullptr);
    }
    template<typename T> std::vector<T> read(Tex &tex) {
        D3D11_TEXTURE2D_DESC td{}; tex.texture->GetDesc(&td);
        td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> stage;
        hr(device->CreateTexture2D(&td, nullptr, &stage), "Create staging texture failed");
        ctx->CopyResource(stage.Get(), tex.texture.Get());
        D3D11_MAPPED_SUBRESOURCE ms{};
        hr(ctx->Map(stage.Get(), 0, D3D11_MAP_READ, 0, &ms), "Map staging texture failed");
        std::vector<T> output(td.Width * td.Height);
        for (UINT y = 0; y < td.Height; ++y) memcpy(output.data() + y * td.Width, static_cast<const uint8_t *>(ms.pData) + y * ms.RowPitch, td.Width * sizeof(T));
        ctx->Unmap(stage.Get(), 0);
        return output;
    }
};
static Cb preset(int method, int quality, float hx = 0.7002075f, float hy = 0.5251556f) {
    const float radius[][4] = { {5,8,11,15}, {8,10,13,16}, {10,13,17,21}, {9,12,16,20} };
    const float directions[][4] = { {8,12,18,24}, {4,6,8,10}, {6,8,10,12}, {4,6,8,8} };
    Cb c;
    c.p0[0] = 1.0f / W; c.p0[1] = 1.0f / H; c.p0[2] = W; c.p0[3] = H;
    c.p1[0] = radius[method][quality]; c.p1[1] = directions[method][quality]; c.p1[2] = float(quality + 3);
    c.p1[3] = method == 0 ? .48f + .08f * quality : method == 1 ? .70f + .05f * quality : method == 2 ? .68f + .05f * quality : 1.80f;
    c.p2[0] = method == 3 ? .015f : .025f; c.p2[1] = method == 3 ? .48f : .30f; c.p2[2] = .32f;
    c.p3[0] = hx; c.p3[1] = hy;
    return c;
}
static void finite_mask(const std::vector<float> &mask) {
    for (float value : mask) require(std::isfinite(value) && value >= 0 && value <= 1, "Mask contains a nonfinite/out-of-range value");
}
// A tilted but planar wall cannot occlude itself. Build perspective depth from
// a known view-space plane, rather than mirroring the reconstruction under test.
static void planar_ao_regressions(Oracle &gpu) {
    Tex raw = gpu.texture(DXGI_FORMAT_R32_FLOAT);
    for (float distance : { 1.8f, 3.0f, 8.0f, 60.0f }) {
        std::vector<float> depth(W * H);
        for (UINT y = 0; y < H; ++y) for (UINT x = 0; x < W; ++x) {
            const float vx = ((x + .5f) / W * 2.0f - 1.0f) * .7002075f;
            const float vy = (1.0f - (y + .5f) / H * 2.0f) * .5251556f;
            const float z = distance / (1.0f + .7f * vx + .3f * vy);
            depth[y * W + x] = 1.0f - 1.0f / z;
        }
        Tex plane = gpu.texture(DXGI_FORMAT_R32_FLOAT, depth.data(), false);
        for (int method = 0; method < 4; ++method) for (int quality = 0; quality < 4; ++quality) {
            const Cb c = preset(method, quality);
            gpu.pass(gpu.methods[method].Get(), raw, nullptr, nullptr, plane.srv.Get(), c);
            const auto mask = gpu.read<float>(raw); finite_mask(mask);
            require(std::all_of(mask.begin(), mask.end(), [](float value) { return std::abs(value - 1.0f) < 1e-5f; }),
                    "Unoccluded tilted plane was spuriously darkened");
        }
    }
    std::cout << "PASS: all 16 AO method/quality combinations preserve tilted planes at four distances, including viewport edges\n";
}

// Execute the real 1x1 focus pass. Use unique aperture sample positions so a
// branch/reload silhouette can be varied independently from the aimed surface.
static void focus_regressions(Oracle &gpu, const std::string &hlsl) {
    constexpr UINT width = 400, height = 300;
    auto shader = gpu.ps(hlsl, "focus_ps");
    auto coc_shader = gpu.ps(hlsl, "coc_ps");
    Tex result = gpu.texture(DXGI_FORMAT_R32_FLOAT, nullptr, true, 1, 1);
    Cb c; c.p0[0] = 1.0f / width; c.p0[1] = 1.0f / height; c.p0[2] = width; c.p0[3] = height;
    c.p1[2] = 1.0f; // acquire the selected surface directly, like the first valid frame
    auto sample = [&](const std::array<float, 25> &aperture, float previous, float alpha = 1.0f) {
        std::vector<float> depth(width * height, 1.0f);
        for (int j = -2; j <= 2; ++j) for (int i = -2; i <= 2; ++i) {
            const UINT x = UINT((.5f + i * .010f * height / width) * width);
            const UINT y = UINT((.5f + j * .010f) * height);
            // Cover the neighbouring texel too: float shader/sample rounding at
            // integral texel boundaries must not alter the synthetic surface.
            for (UINT oy = 0; oy < 2; ++oy) for (UINT ox = 0; ox < 2; ++ox)
                depth[(y - oy) * width + x - ox] = aperture[(j + 2) * 5 + i + 2];
        }
        Tex z = gpu.texture(DXGI_FORMAT_R32_FLOAT, depth.data(), false, width, height);
        Tex prev = gpu.texture(DXGI_FORMAT_R32_FLOAT, &previous, false, 1, 1);
        c.p1[2] = alpha;
        gpu.pass(shader.Get(), result, nullptr, nullptr, z.srv.Get(), c, prev.srv.Get());
        const float focus = gpu.read<float>(result)[0];
        require(std::isfinite(focus) && focus >= 0.0f && focus <= 1.0f, "Autofocus produced invalid depth");
        return focus;
    };
    std::array<float, 25> aperture;
    aperture.fill(.9f);
    require(std::abs(sample(aperture, -1.0f) - .9f) < 1e-6f, "First focus did not acquire an aimed surface");
    for (int i = 0; i < 10; ++i) aperture[i] = .2f;
    require(std::abs(sample(aperture, .9f) - .9f) < 1e-6f, "Minority foreground silhouette pulled focus off background");
    aperture.fill(.9f);
    // A deliberately aimed foreground object occupies all central 3x3 samples
    // plus its four immediate neighbouring points: a majority of the aperture.
    for (int j = 1; j <= 3; ++j) for (int i = 1; i <= 3; ++i) aperture[j * 5 + i] = .2f;
    for (int i : {2, 10, 14, 22}) aperture[i] = .2f;
    require(std::abs(sample(aperture, .9f) - .2f) < 1e-6f, "Aimed foreground subject could not acquire focus");
    require(std::abs(sample(aperture, .9f, .25f) - .725f) < 1e-6f, "Focus transition did not preserve temporal smoothing");
    aperture.fill(1.0f);
    require(std::abs(sample(aperture, .9f) - .9f) < 1e-6f, "Sky discarded established focus");
    require(sample(aperture, -1.0f) == 1.0f, "Sky on first frame did not remain sharp");
    aperture[12] = .8f;
    require(std::abs(sample(aperture, -1.0f) - .8f) < 1e-6f, "Small aimed subject against sky was discarded");
    aperture[0] = -1.0f; aperture[1] = std::nanf(""); aperture[2] = INFINITY;
    require(std::abs(sample(aperture, -1.0f) - .8f) < 1e-6f, "Invalid depth poisoned focus selection");
    aperture.fill(.9f);
    require(std::abs(sample(aperture, std::nanf("")) - .9f) < 1e-6f, "Invalid history poisoned focus");
    // Verify the visible consequence: the background remains sharp after a
    // minority foreground intrusion instead of becoming fully out of focus.
    for (int i = 0; i < 10; ++i) aperture[i] = .2f;
    const float stable = sample(aperture, .9f);
    std::vector<float> background(W * H, .9f);
    Tex z = gpu.texture(DXGI_FORMAT_R32_FLOAT, background.data(), false);
    Tex focus = gpu.texture(DXGI_FORMAT_R32_FLOAT, &stable, false, 1, 1);
    Tex coc = gpu.texture(DXGI_FORMAT_R32_FLOAT);
    c.p1[3] = 140.0f; c.p2[0] = .002f;
    gpu.pass(coc_shader.Get(), coc, nullptr, nullptr, z.srv.Get(), c, focus.srv.Get());
    const auto mask = gpu.read<float>(coc);
    require(std::all_of(mask.begin(), mask.end(), [](float value) { return value == 0.0f; }),
            "Foreground edge spuriously blurred the aimed background");
    std::cout << "PASS: actual focus/CoC shaders preserve background across minority occluders, acquire foreground subjects, reject invalid samples and retain smoothing/sky behavior\n";
}

int main(int argc, char **argv) try {
    require(argc == 2, "Usage: ao_warp_test.exe <kelvin_gpu_post.inc>");
    const std::string hlsl = source_hlsl(argv[1]);
    Oracle gpu(hlsl);
    planar_ao_regressions(gpu);
    focus_regressions(gpu, hlsl);
    std::vector<float> flat(W * H, 8.0f / 9.0f), sky(W * H, 1.0f), corner(W * H);
    for (UINT y = 0; y < H; ++y) for (UINT x = 0; x < W; ++x)
        corner[y * W + x] = y < 8 ? 1.0f : x < W / 2 ? 6.7f / 7.7f : 8.0f / 9.0f;
    Tex flat_d = gpu.texture(DXGI_FORMAT_R32_FLOAT, flat.data(), false);
    Tex sky_d = gpu.texture(DXGI_FORMAT_R32_FLOAT, sky.data(), false);
    Tex corner_d = gpu.texture(DXGI_FORMAT_R32_FLOAT, corner.data(), false);
    // Float masks catch NaNs before UNORM conversion can hide them.
    Tex raw = gpu.texture(DXGI_FORMAT_R32_FLOAT), scratch = gpu.texture(DXGI_FORMAT_R32_FLOAT);
    Tex raw8 = gpu.texture(DXGI_FORMAT_R8_UNORM), scratch8 = gpu.texture(DXGI_FORMAT_R8_UNORM);
    std::vector<uint32_t> colours(W * H, 0x89DCBEA0u);
    Tex scene = gpu.texture(DXGI_FORMAT_B8G8R8A8_UNORM, colours.data(), false);
    Tex output = gpu.texture(DXGI_FORMAT_B8G8R8A8_UNORM);
    const char *names[] = { "SSAO", "HBAO", "HBAO+", "GTAO" };
    std::array<double, 4> high_occlusion{};
    for (int method = 0; method < 4; ++method) {
        std::array<double, 4> quality_occlusion{};
        for (int quality = 0; quality < 4; ++quality) {
            Cb c = preset(method, quality);
            for (Tex *depth : { &flat_d, &sky_d }) {
                gpu.pass(gpu.methods[method].Get(), raw, nullptr, nullptr, depth->srv.Get(), c);
                auto mask = gpu.read<float>(raw); finite_mask(mask);
                require(std::all_of(mask.begin(), mask.end(), [](float v) { return std::abs(v - 1) < 1e-6f; }), "Flat/sky geometry is spuriously occluded");
            }
            gpu.pass(gpu.methods[method].Get(), raw, nullptr, nullptr, corner_d.srv.Get(), c);
            auto original = gpu.read<float>(raw); finite_mask(original);
            gpu.pass(gpu.blur_h.Get(), scratch, nullptr, raw.srv.Get(), corner_d.srv.Get(), c);
            gpu.pass(gpu.blur_v.Get(), raw, nullptr, scratch.srv.Get(), corner_d.srv.Get(), c);
            auto mask = gpu.read<float>(raw); finite_mask(mask);
            double sum = 0; UINT occluded = 0;
            for (UINT y = 0; y < H; ++y) for (UINT x = 0; x < W; ++x) {
                float v = mask[y * W + x];
                if (y < 8) require(v == 1, "Bilateral blur leaked onto sky");
                sum += 1.0 - v; occluded += v < .999f;
            }
            require(occluded > 0, "Corner produced an all-white AO mask");
            quality_occlusion[quality] = sum;
            // Repeat with the renderer's actual R8 mask/blur textures before testing visible compositing.
            gpu.pass(gpu.methods[method].Get(), raw8, nullptr, nullptr, corner_d.srv.Get(), c);
            gpu.pass(gpu.blur_h.Get(), scratch8, nullptr, raw8.srv.Get(), corner_d.srv.Get(), c);
            gpu.pass(gpu.blur_v.Get(), raw8, nullptr, scratch8.srv.Get(), corner_d.srv.Get(), c);
            auto bytes = gpu.read<uint8_t>(raw8);
            require(std::any_of(bytes.begin(), bytes.end(), [](uint8_t v) { return v < 255; }), "R8 conversion lost all AO");
            for (UINT i = 0; i < 8 * W; ++i) require(bytes[i] == 255, "R8 bilateral blur leaked onto sky");
            gpu.pass(gpu.composite.Get(), output, scene.srv.Get(), raw8.srv.Get(), nullptr, c);
            auto after = gpu.read<uint32_t>(output);
            UINT changed = 0;
            for (size_t i = 0; i < after.size(); ++i) {
                require((after[i] >> 24) == 0x89, "Composite changed scene alpha");
                for (int ch = 0; ch < 3; ++ch) require(((after[i] >> (ch * 8)) & 255) <= ((colours[i] >> (ch * 8)) & 255), "AO composite brightened scene");
                changed += after[i] != colours[i];
            }
            require(changed > 0, "AO mask produced no visible scene change");
            std::cout << names[method] << " quality=" << quality << " mean_occlusion=" << std::setprecision(8)
                      << sum / mask.size() << " occluded=" << occluded << " changed=" << changed << '\n';
        }
        require(std::abs(quality_occlusion[0] - quality_occlusion[3]) > 1e-3, "Low/Ultra sampling has no real difference");
        high_occlusion[method] = quality_occlusion[2];
    }
    for (int i = 0; i < 4; ++i) for (int j = i + 1; j < 4; ++j)
        require(std::abs(high_occlusion[i] - high_occlusion[j]) > 1e-3, "Different method kernels produced identical masks");
    Cb narrow = preset(3, 2), wide = preset(3, 2, 2.78076f, 1.19175f);
    gpu.pass(gpu.methods[3].Get(), raw, nullptr, nullptr, corner_d.srv.Get(), narrow);
    auto n = gpu.read<float>(raw);
    gpu.pass(gpu.methods[3].Get(), raw, nullptr, nullptr, corner_d.srv.Get(), wide);
    auto w = gpu.read<float>(raw);
    finite_mask(w);
    double camera_diff = 0;
    for (size_t i = 0; i < n.size(); ++i) camera_diff += std::abs(n[i] - w[i]);
    require(camera_diff > 1e-3, "Real camera half-frustum has no effect on AO reconstruction");
    std::cout << "PASS: 13 shaders compiled; all 16 method/quality combinations passed flat, sky, corner, float/R8 blur and composite checks; camera difference="
              << camera_diff << '\n';
    return 0;
} catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
