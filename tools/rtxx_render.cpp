// rtxx_render — renderiza un WAV a través del motor de Retroexx con parámetros del host.
//
// Es la herramienta de medida: no dibuja nada y no toca archivos fuera de los que se le
// piden. Uso:
//
//   bin/rtxx_render --signal in.wav --params p.json --out out.wav --report r.json \
//                   [--sr 44100] [--block 64] [--tail 0]
//
// `--params` es un JSON plano {"<id>": <valor normalizado 0..1>} con los ids del host
// (0..57). Los parámetros sin ley verificada se
// guardan y se reportan como no implementados: no se inventa su conversión.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "engine/Engine.h"
#include "io/WavFile.h"
#include "model/ParamStore.h"
#include "product/ProductInfo.h"

namespace {

struct Options {
    std::string signal;
    std::string paramsPath;
    std::string out;
    std::string report;
    int sampleRate = 44100;
    int block = 64;
    double tailSeconds = 0.0;
};

void usage() {
    std::printf(
        "rtxx_render — render offline de %s (%s)\n\n"
        "  --signal <in.wav>     entrada (float32 o PCM16/24)\n"
        "  --params <p.json>     {\"<id host>\": valor 0..1}\n"
        "  --out <out.wav>       salida float32\n"
        "  --report <r.json>     reporte de la corrida\n"
        "  --sr <hz>             sample rate (44100)\n"
        "  --block <n>           tamaño de bloque del host (64)\n"
        "  --tail <s>            segundos de cola a renderizar (0)\n",
        rtxx::kProductName, rtxx::kManufacturer);
}

// Parser del subconjunto de JSON que usan los ficheros de parámetros. No es un parser
// general: números y claves enteras, con errores explícitos en vez de silencio.
bool parseParams(const std::string& path, std::map<int, float>& out, std::string& error) {
    std::ifstream f(path);
    if (!f) {
        error = "no se puede abrir " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string s = ss.str();
    size_t i = 0;
    auto skip = [&]() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    };
    skip();
    if (i >= s.size() || s[i] != '{') {
        error = "se esperaba un objeto JSON en " + path;
        return false;
    }
    ++i;
    while (true) {
        skip();
        if (i < s.size() && s[i] == '}') break;
        if (i >= s.size() || s[i] != '"') {
            error = "se esperaba una clave entre comillas en " + path;
            return false;
        }
        ++i;
        std::string key;
        while (i < s.size() && s[i] != '"') key += s[i++];
        if (i >= s.size()) {
            error = "clave sin cerrar en " + path;
            return false;
        }
        ++i;
        skip();
        if (i >= s.size() || s[i] != ':') {
            error = "se esperaba ':' en " + path;
            return false;
        }
        ++i;
        skip();
        char* end = nullptr;
        const double v = std::strtod(s.c_str() + i, &end);
        if (end == s.c_str() + i) {
            error = "valor no numérico en " + path;
            return false;
        }
        i = static_cast<size_t>(end - s.c_str());
        int id = -1;
        try {
            id = std::stoi(key);
        } catch (...) {
            error = "clave no entera en " + path + ": " + key;
            return false;
        }
        out[id] = static_cast<float>(v);
        skip();
        if (i < s.size() && s[i] == ',') ++i;
    }
    return true;
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "falta valor para %s\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--signal") o.signal = next("--signal");
        else if (a == "--params") o.paramsPath = next("--params");
        else if (a == "--out") o.out = next("--out");
        else if (a == "--report") o.report = next("--report");
        else if (a == "--sr") o.sampleRate = std::atoi(next("--sr").c_str());
        else if (a == "--block") o.block = std::atoi(next("--block").c_str());
        else if (a == "--tail") o.tailSeconds = std::atof(next("--tail").c_str());
        else if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else {
            std::fprintf(stderr, "opción desconocida: %s\n", a.c_str());
            return 2;
        }
    }
    if (o.signal.empty() || o.out.empty()) {
        usage();
        return 2;
    }

    rtxx::AudioFile in;
    std::string error;
    if (!rtxx::readWav(o.signal, in, error)) {
        std::fprintf(stderr, "entrada: %s\n", error.c_str());
        return 1;
    }

    std::map<int, float> hostParams;
    if (!o.paramsPath.empty() && !parseParams(o.paramsPath, hostParams, error)) {
        std::fprintf(stderr, "parámetros: %s\n", error.c_str());
        return 1;
    }

    rtxx::ParamStore store;
    store.resetToDefaults();
    int applied = 0;
    std::vector<int> notImplemented;
    for (const auto& kv : hostParams) {
        if (store.setHost(kv.first, kv.second)) ++applied;
        else notImplemented.push_back(kv.first);
    }

    rtxx::Engine engine(store);
    if (!engine.build(error)) {
        std::fprintf(stderr, "grafo: %s\n", error.c_str());
        return 1;
    }
    engine.prepare(o.sampleRate, o.block);

    const int channels = 2;
    const int inFrames = in.frames;
    const int tailFrames = static_cast<int>(o.tailSeconds * o.sampleRate);
    const int outFrames = inFrames + tailFrames;
    std::vector<float> out(static_cast<size_t>(outFrames) * channels, 0.0f);

    std::vector<float> l(static_cast<size_t>(inFrames)), r(static_cast<size_t>(inFrames));
    rtxx::extractChannel(in, 0, l);
    rtxx::extractChannel(in, in.channels > 1 ? 1 : 0, r);

    const int block = o.block > 0 ? o.block : 64;
    std::vector<float> bl(static_cast<size_t>(block)), br(static_cast<size_t>(block));
    int done = 0;
    while (done < outFrames) {
        const int n = (done + block <= outFrames) ? block : (outFrames - done);
        for (int i = 0; i < n; ++i) {
            const int src = done + i;
            bl[static_cast<size_t>(i)] = src < inFrames ? l[static_cast<size_t>(src)] : 0.0f;
            br[static_cast<size_t>(i)] = src < inFrames ? r[static_cast<size_t>(src)] : 0.0f;
        }
        rtxx::PortData pin;
        pin.numChannels = channels;
        pin.numSamples = n;
        pin.ch[0] = bl.data();
        pin.ch[1] = br.data();
        rtxx::PortData pout;
        pout.numChannels = channels;
        pout.numSamples = n;
        pout.ch[0] = out.data() + static_cast<size_t>(done) * channels;
        pout.ch[1] = out.data() + static_cast<size_t>(done) * channels + 1;
        // El motor escribe entrelazado por canal: se usan dos acumuladores locales.
        std::vector<float> ol(static_cast<size_t>(n)), orr(static_cast<size_t>(n));
        float* chans[2] = {ol.data(), orr.data()};
        rtxx::PortData poutLocal;
        poutLocal.numChannels = channels;
        poutLocal.numSamples = n;
        poutLocal.ch[0] = chans[0];
        poutLocal.ch[1] = chans[1];
        engine.process(pin, poutLocal, n);
        for (int i = 0; i < n; ++i) {
            const size_t idx = (static_cast<size_t>(done) + static_cast<size_t>(i)) * channels;
            out[idx] = chans[0][i];
            out[idx + 1] = chans[1][i];
        }
        done += n;
    }

    if (!rtxx::writeWavF32(o.out, out.data(), outFrames, channels, o.sampleRate, error)) {
        std::fprintf(stderr, "salida: %s\n", error.c_str());
        return 1;
    }

    const rtxx::ParamStore::Coverage cov = store.coverage();
    if (!o.report.empty()) {
        std::ofstream r(o.report);
        r << "{\n";
        r << "  \"schema\": \"retroexx.render/1\",\n";
        r << "  \"product\": \"" << rtxx::kProductName << "\",\n";
        r << "  \"manufacturer\": \"" << rtxx::kManufacturer << "\",\n";
        r << "  \"version\": \"" << rtxx::kVersion << "\",\n";
        r << "  \"config\": {\"sample_rate\": " << o.sampleRate << ", \"block\": " << block
          << ", \"channels\": " << channels << ", \"input_frames\": " << inFrames
          << ", \"output_frames\": " << outFrames << ", \"tail_frames\": " << tailFrames << "},\n";
        r << "  \"latency_samples\": " << rtxx::Engine::latencySamples() << ",\n";
        r << "  \"latency_seconds\": " << (static_cast<double>(rtxx::Engine::latencySamples()) / o.sampleRate) << ",\n";
        r << "  \"params\": {\"applied\": " << applied << ", \"not_implemented\": [";
        for (size_t i = 0; i < notImplemented.size(); ++i)
            r << (i ? ", " : "") << notImplemented[i];
        r << "]},\n";
        r << "  \"coverage\": {\"total\": " << cov.total << ", \"verified\": " << cov.verified
          << ", \"identity\": " << cov.identity << ", \"unverified\": " << cov.unverified << "},\n";
        r << "  \"nodes\": [";
        for (size_t i = 0; i < engine.nodeNames().size(); ++i)
            r << (i ? ", " : "") << "\"" << jsonEscape(engine.nodeNames()[i]) << "\"";
        r << "],\n";
        int implemented = 0;
        for (const auto& b : engine.blockStatus()) implemented += b.implemented ? 1 : 0;
        r << "  \"blocks\": {\"implemented\": " << implemented << ", \"identity\": "
          << (static_cast<int>(engine.blockStatus().size()) - implemented) << ", \"detail\": [";
        for (size_t i = 0; i < engine.blockStatus().size(); ++i) {
            const auto& b = engine.blockStatus()[i];
            r << (i ? ", " : "") << "{\"node\": \"" << jsonEscape(b.node) << "\", \"class\": \""
              << jsonEscape(b.className) << "\", \"status\": \"" << (b.implemented ? "implemented" : "identity")
              << "\"}";
        }
        r << "]}\n}\n";
    }

    std::printf("%s %s: %d frames (%d in + %d tail) @ %d Hz, latencia %d muestras, %d/%d params con ley\n",
                rtxx::kProductName, rtxx::kVersion, outFrames, inFrames, tailFrames, o.sampleRate,
                rtxx::Engine::latencySamples(), cov.verified + cov.identity, cov.total);
    return 0;
}
