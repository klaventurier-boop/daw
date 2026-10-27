// Studio Voice Pro — lecture et écriture WAV, implémentation maison.
//
// Structure RIFF/WAVE prise en charge :
//   "RIFF" <taille> "WAVE" puis des chunks "id" <taille> <données> (taille
//   arrondie au mot de 16 bits pour la lecture suivante).
//   - "fmt "  : format 1 (PCM entier) ou 3 (flottant IEEE), 16 ou 18 octets.
//   - "data"  : échantillons entrelacés.
//   - tout autre chunk est ignoré (LIST, fact, bext...).
//
// Toutes les lectures/écritures sont en petit-boutiste (little endian), comme
// l'exige la spécification WAVE.
#include "svp/io/WavFile.h"

#include <array>
#include <cstring>
#include <fstream>
#include <vector>

#include "svp/audio/Convert.h"

namespace svp::io {

namespace {

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatIeeeFloat = 3;

std::uint16_t readU16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t readU32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
           | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

void writeU16(std::ostream& out, std::uint16_t value)
{
    const std::array<char, 2> bytes{static_cast<char>(value & 0xFF), static_cast<char>((value >> 8) & 0xFF)};
    out.write(bytes.data(), bytes.size());
}

void writeU32(std::ostream& out, std::uint32_t value)
{
    const std::array<char, 4> bytes{static_cast<char>(value & 0xFF),
                                    static_cast<char>((value >> 8) & 0xFF),
                                    static_cast<char>((value >> 16) & 0xFF),
                                    static_cast<char>((value >> 24) & 0xFF)};
    out.write(bytes.data(), bytes.size());
}

void writeTag(std::ostream& out, const char (&tag)[5])
{
    out.write(tag, 4);
}

} // namespace

int WavFormat::bitsPerSample() const noexcept
{
    switch (encoding) {
        case WavEncoding::Pcm16:
            return 16;
        case WavEncoding::Pcm24:
            return 24;
        case WavEncoding::Float32:
            return 32;
    }
    return 16;
}

int WavFormat::bytesPerSample() const noexcept
{
    return bitsPerSample() / 8;
}

WavReadResult readWavFile(const std::string& path)
{
    WavReadResult result;

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {false, "fichier introuvable ou illisible : " + path, {}, {}};

    in.seekg(0, std::ios::end);
    const auto fileSize = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (fileSize < 44)
        return {false, "fichier trop court pour un WAV valide", {}, {}};

    std::vector<std::uint8_t> header(std::min<std::size_t>(fileSize, 4096));
    in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (in.gcount() < 12)
        return {false, "en-tête RIFF tronqué", {}, {}};
    header.resize(static_cast<std::size_t>(in.gcount()));

    if (std::memcmp(header.data(), "RIFF", 4) != 0 || std::memcmp(header.data() + 8, "WAVE", 4) != 0)
        return {false, "ce n'est pas un fichier RIFF/WAVE", {}, {}};

    WavFormat format;
    std::uint16_t audioFormat = 0;
    std::uint16_t bitsPerSample = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;
    bool haveFormat = false;
    bool haveData = false;

    std::size_t cursor = 12;
    while (cursor + 8 <= header.size()) {
        const std::uint8_t* chunk = header.data() + cursor;
        const std::uint32_t chunkSize = readU32(chunk + 4);
        const std::size_t body = cursor + 8;

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (body + 16 > header.size())
                return {false, "chunk fmt tronqué", {}, {}};
            audioFormat = readU16(header.data() + body);
            format.channels = readU16(header.data() + body + 2);
            format.sampleRate = static_cast<int>(readU32(header.data() + body + 4));
            bitsPerSample = readU16(header.data() + body + 14);
            haveFormat = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataOffset = body;
            dataSize = chunkSize;
            haveData = true;
            break; // les données suivent le chunk data : inutile de continuer
        }

        cursor = body + chunkSize + (chunkSize & 1u);
    }

    if (!haveFormat || !haveData)
        return {false, "chunk fmt ou data absent", {}, {}};
    if (format.channels <= 0)
        return {false, "nombre de canaux invalide", {}, {}};

    if (audioFormat == kFormatPcm && bitsPerSample == 16)
        format.encoding = WavEncoding::Pcm16;
    else if (audioFormat == kFormatPcm && bitsPerSample == 24)
        format.encoding = WavEncoding::Pcm24;
    else if (audioFormat == kFormatIeeeFloat && bitsPerSample == 32)
        format.encoding = WavEncoding::Float32;
    else if (audioFormat == kFormatPcm && bitsPerSample == 32)
        format.encoding = WavEncoding::Pcm24; // PCM 32 bits non prévu : refusé explicitement
    else
        return {false,
                "format non pris en charge (format=" + std::to_string(audioFormat)
                    + ", bits=" + std::to_string(bitsPerSample) + ") ; v0.1 accepte PCM 16, PCM 24, float 32",
                {},
                {}};

    if (audioFormat == kFormatPcm && bitsPerSample == 32)
        return {false, "PCM 32 bits entier non pris en charge en v0.1", {}, {}};

    const std::size_t frameBytes = static_cast<std::size_t>(format.bytesPerSample())
                                   * static_cast<std::size_t>(format.channels);
    if (dataOffset + dataSize > fileSize)
        dataSize = (fileSize > dataOffset) ? (fileSize - dataOffset) : 0;
    const int frames = static_cast<int>(dataSize / frameBytes);

    if (frames <= 0)
        return {false, "chunk data vide", {}, {}};

    result.buffer.resize(format.channels, frames);

    std::vector<std::uint8_t> raw(dataSize);
    in.seekg(static_cast<std::streamoff>(dataOffset), std::ios::beg);
    in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    const std::size_t bytesRead = static_cast<std::size_t>(in.gcount());
    if (bytesRead < raw.size())
        raw.resize(bytesRead);

    const std::size_t stride = static_cast<std::size_t>(format.bytesPerSample());
    for (int frame = 0; frame < frames; ++frame) {
        for (int ch = 0; ch < format.channels; ++ch) {
            const std::size_t index = (static_cast<std::size_t>(frame) * static_cast<std::size_t>(format.channels)
                                       + static_cast<std::size_t>(ch))
                                      * stride;
            if (index + stride > raw.size())
                break;
            const std::uint8_t* sample = raw.data() + index;
            float value = 0.0f;
            switch (format.encoding) {
                case WavEncoding::Pcm16:
                    value = audio::pcm16ToFloat(static_cast<std::int16_t>(readU16(sample)));
                    break;
                case WavEncoding::Pcm24: {
                    std::uint32_t raw24 = static_cast<std::uint32_t>(sample[0])
                                          | (static_cast<std::uint32_t>(sample[1]) << 8)
                                          | (static_cast<std::uint32_t>(sample[2]) << 16);
                    if ((raw24 & 0x00800000u) != 0)
                        raw24 |= 0xFF000000u; // extension de signe 24 -> 32 bits
                    value = audio::pcm24ToFloat(static_cast<std::int32_t>(raw24));
                    break;
                }
                case WavEncoding::Float32: {
                    float f = 0.0f;
                    std::memcpy(&f, sample, sizeof(float));
                    value = f;
                    break;
                }
            }
            result.buffer.setSample(ch, frame, value);
        }
    }

    result.ok = true;
    result.format = format;
    return result;
}

bool writeWavFile(const std::string& path,
                  const audio::AudioBuffer& buffer,
                  const WavFormat& format,
                  std::string* error)
{
    auto fail = [error](const std::string& message) {
        if (error != nullptr)
            *error = message;
        return false;
    };

    if (buffer.empty())
        return fail("buffer vide : rien à écrire");
    if (format.channels <= 0 || buffer.channels() != format.channels)
        return fail("incohérence du nombre de canaux entre le buffer et le format");
    if (format.sampleRate <= 0)
        return fail("taux d'échantillonnage invalide");

    const std::uint16_t bytesPerSample = static_cast<std::uint16_t>(format.bytesPerSample());
    const std::uint32_t dataSize = static_cast<std::uint32_t>(buffer.frames())
                                   * static_cast<std::uint32_t>(bytesPerSample)
                                   * static_cast<std::uint32_t>(format.channels);

    // Pour le flottant, on écrit un chunk fmt de 18 octets (avec cbSize = 0),
    // forme la plus largement acceptée par les lecteurs WAV.
    const std::uint32_t fmtSize = (format.encoding == WavEncoding::Float32) ? 18u : 16u;
    const std::uint32_t riffSize = 4u + (8u + fmtSize) + (8u + dataSize);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return fail("impossible de créer le fichier : " + path);

    writeTag(out, "RIFF");
    writeU32(out, riffSize);
    writeTag(out, "WAVE");

    writeTag(out, "fmt ");
    writeU32(out, fmtSize);
    writeU16(out, format.encoding == WavEncoding::Float32 ? kFormatIeeeFloat : kFormatPcm);
    writeU16(out, static_cast<std::uint16_t>(format.channels));
    writeU32(out, static_cast<std::uint32_t>(format.sampleRate));
    writeU32(out, static_cast<std::uint32_t>(format.sampleRate) * bytesPerSample
                      * static_cast<std::uint32_t>(format.channels)); // débit moyen
    writeU16(out, static_cast<std::uint16_t>(bytesPerSample * format.channels)); // alignement bloc
    writeU16(out, static_cast<std::uint16_t>(bytesPerSample * 8));
    if (fmtSize == 18u)
        writeU16(out, 0); // cbSize

    writeTag(out, "data");
    writeU32(out, dataSize);

    // Les échantillons du fichier WAV sont ENTRELACÉS (trame par trame), alors
    // que le buffer interne est en canal majeur : la conversion est explicite.
    std::vector<char> scratch;
    scratch.reserve(static_cast<std::size_t>(dataSize));

    for (int frame = 0; frame < buffer.frames(); ++frame) {
        for (int ch = 0; ch < format.channels; ++ch) {
            const float value = buffer.sample(ch, frame);
            switch (format.encoding) {
                case WavEncoding::Pcm16: {
                    const std::int16_t v = audio::floatToPcm16(value);
                    scratch.push_back(static_cast<char>(v & 0xFF));
                    scratch.push_back(static_cast<char>((v >> 8) & 0xFF));
                    break;
                }
                case WavEncoding::Pcm24: {
                    const std::int32_t v = audio::floatToPcm24(value);
                    scratch.push_back(static_cast<char>(v & 0xFF));
                    scratch.push_back(static_cast<char>((v >> 8) & 0xFF));
                    scratch.push_back(static_cast<char>((v >> 16) & 0xFF));
                    break;
                }
                case WavEncoding::Float32: {
                    char bytes[sizeof(float)];
                    std::memcpy(bytes, &value, sizeof(float));
                    scratch.insert(scratch.end(), bytes, bytes + sizeof(float));
                    break;
                }
            }
        }

        // Écriture par paquets de 64 Ko pour éviter un appel système par trame.
        if (scratch.size() >= 65536u) {
            out.write(scratch.data(), static_cast<std::streamsize>(scratch.size()));
            scratch.clear();
        }
    }

    if (!scratch.empty())
        out.write(scratch.data(), static_cast<std::streamsize>(scratch.size()));

    out.flush();
    if (!out)
        return fail("erreur d'écriture sur le disque : " + path);
    return true;
}

} // namespace svp::io
