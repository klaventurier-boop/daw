// Studio Voice Pro — svp-render : traitement hors ligne d'un fichier WAV.
// Usage :
//   svp-render --in a.wav --out b.wav --gain -6 --pan -0.3 --meter
// Le fichier de sortie reprend le taux d'échantillonnage et le nombre de
// canaux de l'entrée ; la profondeur de sortie est choisie par --bits
// (16, 24 ou 32f) et vaut par défaut celle de l'entrée.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "svp/audio/Convert.h"
#include "svp/dsp/GainPan.h"
#include "svp/io/WavFile.h"
#include "svp/metering/Meter.h"

namespace {

struct Options {
    std::string inPath;
    std::string outPath;
    double gainDb = 0.0;
    double pan = 0.0;
    bool meter = false;
    bool mute = false;
    bool haveBits = false;
    svp::io::WavEncoding encoding = svp::io::WavEncoding::Float32;
};

void printUsage()
{
    std::printf(
        "svp-render 0.1 — traitement WAV hors ligne (Studio Voice Pro)\n"
        "  --in <fichier.wav>    fichier source (obligatoire)\n"
        "  --out <fichier.wav>   fichier destination (obligatoire)\n"
        "  --gain <dBFS>         gain appliqué, ex. -6 (défaut 0)\n"
        "  --pan <valeur>        panoramique de -1 (gauche) à +1 (droite)\n"
        "  --bits <16|24|32f>    profondeur de sortie (défaut : celle de l'entrée)\n"
        "  --mute                silencie les pistes (test de la règle mute)\n"
        "  --meter               imprime crête et RMS en dBFS avant/après\n"
        "  --help                cette aide\n");
}

bool parseEncoding(const char* text, svp::io::WavEncoding& out)
{
    if (std::strcmp(text, "16") == 0) {
        out = svp::io::WavEncoding::Pcm16;
        return true;
    }
    if (std::strcmp(text, "24") == 0) {
        out = svp::io::WavEncoding::Pcm24;
        return true;
    }
    if (std::strcmp(text, "32f") == 0 || std::strcmp(text, "32") == 0) {
        out = svp::io::WavEncoding::Float32;
        return true;
    }
    return false;
}

const char* encodingName(svp::io::WavEncoding encoding)
{
    switch (encoding) {
        case svp::io::WavEncoding::Pcm16:
            return "PCM 16 bits";
        case svp::io::WavEncoding::Pcm24:
            return "PCM 24 bits";
        case svp::io::WavEncoding::Float32:
            return "flottant 32 bits";
    }
    return "inconnu";
}

int fail(const std::string& message)
{
    std::fprintf(stderr, "svp-render : erreur : %s\n", message.c_str());
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "svp-render : %s attend une valeur\n", name);
                std::exit(2);
            }
            return argv[++i];
        };

        if (std::strcmp(arg, "--in") == 0)
            options.inPath = next("--in");
        else if (std::strcmp(arg, "--out") == 0)
            options.outPath = next("--out");
        else if (std::strcmp(arg, "--gain") == 0)
            options.gainDb = std::atof(next("--gain"));
        else if (std::strcmp(arg, "--pan") == 0)
            options.pan = std::atof(next("--pan"));
        else if (std::strcmp(arg, "--bits") == 0) {
            if (!parseEncoding(next("--bits"), options.encoding)) {
                printUsage();
                return 2;
            }
            options.haveBits = true;
        } else if (std::strcmp(arg, "--meter") == 0)
            options.meter = true;
        else if (std::strcmp(arg, "--mute") == 0)
            options.mute = true;
        else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            printUsage();
            return 0;
        } else {
            std::fprintf(stderr, "svp-render : argument inconnu « %s »\n", arg);
            printUsage();
            return 2;
        }
    }

    if (options.inPath.empty() || options.outPath.empty()) {
        printUsage();
        return 2;
    }

    svp::io::WavReadResult read = svp::io::readWavFile(options.inPath);
    if (!read.ok)
        return fail("lecture impossible : " + read.error);

    std::printf("entrée      : %s\n", options.inPath.c_str());
    std::printf("  format    : %s, %d Hz, %d canal(aux), %d trames (%.3f s)\n",
                encodingName(read.format.encoding),
                read.format.sampleRate,
                read.format.channels,
                read.buffer.frames(),
                static_cast<double>(read.buffer.frames()) / read.format.sampleRate);

    const svp::metering::MeterValues before = svp::metering::measureBlock(read.buffer);
    // Traitement NON DESTRUCTIF : le media lu reste intact, le resultat part
    // dans un buffer de sortie. Regle « on lit une source, on ecrit une
    // destination » — voir dsp/GainPan.h (les variantes en place sont
    // reservees aux tampons de travail, jamais a un media charge).
    svp::audio::AudioBuffer processed;
    processed.resize(read.buffer.channels(), read.buffer.frames());
    if (options.mute) {
        processed.clear(); // silence explicite en sortie
    } else {
        svp::dsp::applyGainTo(read.buffer.constView(), processed.view(),
                              static_cast<float>(svp::audio::decibelsToGain(options.gainDb)));
        // Le panoramique est purement echantillon par echantillon :
        // source == destination ne pose aucun probleme ici.
        svp::dsp::applyPanTo(processed.constView(), processed.view(),
                             static_cast<float>(options.pan));
    }
    const svp::metering::MeterValues after = svp::metering::measureBlock(processed);

    if (options.meter) {
        std::printf("mesures AVANT traitement :\n");
        std::printf("  crête = %8.3f dBFS   RMS = %8.3f dBFS%s\n",
                    svp::audio::gainToDecibels(before.peak),
                    svp::audio::gainToDecibels(before.rms),
                    before.clipped ? "   [écrêtage]" : "");
        std::printf("mesures APRÈS traitement (gain %+g dB, pan %+g) :\n", options.gainDb, options.pan);
        std::printf("  crête = %8.3f dBFS   RMS = %8.3f dBFS%s\n",
                    svp::audio::gainToDecibels(after.peak),
                    svp::audio::gainToDecibels(after.rms),
                    after.clipped ? "   [écrêtage]" : "");
        std::printf("  écart de gain mesuré : %+.3f dB\n",
                    svp::audio::gainToDecibels(after.rms) - svp::audio::gainToDecibels(before.rms));
    }

    svp::io::WavFormat outFormat = read.format;
    if (options.haveBits)
        outFormat.encoding = options.encoding;

    std::string error;
    if (!svp::io::writeWavFile(options.outPath, processed, outFormat, &error))
        return fail("écriture impossible : " + error);

    std::printf("sortie      : %s\n", options.outPath.c_str());
    std::printf("  format    : %s, %d Hz, %d canal(aux), %d trames\n",
                encodingName(outFormat.encoding),
                outFormat.sampleRate,
                outFormat.channels,
                read.buffer.frames());
    return 0;
}
