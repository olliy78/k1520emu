/**
 * @file image_codec.cpp
 * @brief Implementierung der Container-Fabrik (Erkennung + Dispatch).
 *
 * @see core/peripherals/floppy_drive/image_codec.h
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/peripherals/floppy_drive/image_codec.h"
#include "core/peripherals/floppy_drive/cqm_codec.h"
#include "core/peripherals/floppy_drive/dmk_codec.h"
#include "core/peripherals/floppy_drive/hfe_codec.h"
#include "core/peripherals/floppy_drive/imd_codec.h"
#include "core/peripherals/floppy_drive/img_codec.h"
#include "core/peripherals/floppy_drive/td0_codec.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

/// @brief Endet @p path (case-insensitiv) auf @p suffix?
bool endetAuf(const std::string& path, const char* suffix) {
    const size_t n = std::strlen(suffix);
    if (path.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(path[path.size() - n + i]))
            != std::tolower(static_cast<unsigned char>(suffix[i])))
            return false;
    }
    return true;
}

}  // namespace

ContainerType ImageCodec::fromExtension(const std::string& path) {
    if (endetAuf(path, ".hfe")) return ContainerType::Hfe;
    if (endetAuf(path, ".dmk")) return ContainerType::Dmk;
    if (endetAuf(path, ".cqm")) return ContainerType::Cqm;
    if (endetAuf(path, ".td0")) return ContainerType::Td0;
    if (endetAuf(path, ".imd")) return ContainerType::Imd;
    return ContainerType::Img;
}

ContainerType ImageCodec::detect(const std::string& path) {
    // HFE traegt eine echte Signatur → hat Vorrang vor der Endung.
    if (HfeCodec::isHfe(path)) return ContainerType::Hfe;
    // CQM traegt Kennung + Kopfpruefbyte → ebenfalls Vorrang vor der Endung.
    if (CqmCodec::isCqm(path)) return ContainerType::Cqm;
    // TD0 (Kennung + Kopf-CRC) und IMD (Kennung + 0x1A) tragen eine Kennung.
    if (Td0Codec::isTd0(path)) return ContainerType::Td0;
    if (ImdCodec::isImd(path)) return ContainerType::Imd;
    // DMK hat keine Magic-Zahl: nur akzeptieren, wenn Endung UND Header passen.
    if (endetAuf(path, ".dmk") && DmkCodec::looksLikeDmk(path)) return ContainerType::Dmk;
    return ContainerType::Img;
}

const char* ImageCodec::name(ContainerType t) {
    switch (t) {
        case ContainerType::Hfe: return "hfe";
        case ContainerType::Dmk: return "dmk";
        case ContainerType::Cqm: return "cqm";
        case ContainerType::Td0: return "td0";
        case ContainerType::Imd: return "imd";
        case ContainerType::Img: break;
    }
    return "img";
}

const char* ImageCodec::extension(ContainerType t) {
    switch (t) {
        case ContainerType::Hfe: return ".hfe";
        case ContainerType::Dmk: return ".dmk";
        case ContainerType::Cqm: return ".cqm";
        case ContainerType::Td0: return ".td0";
        case ContainerType::Imd: return ".imd";
        case ContainerType::Img: break;
    }
    return ".img";
}

bool ImageCodec::selfDescribing(ContainerType t) { return t != ContainerType::Img; }  // Td0/Imd: ja
bool ImageCodec::needsDiskFormat(ContainerType t) { return t == ContainerType::Img; }
bool ImageCodec::writable(ContainerType) { return true; }

bool ImageCodec::load(const std::string& path, ContainerType type,
                      const DiskFormat* fmt, DiskMedium& out, std::string& err) {
    switch (type) {
        case ContainerType::Hfe:
            return HfeCodec::load(path, out, nullptr, err);
        case ContainerType::Dmk:
            return DmkCodec::load(path, out, nullptr, err);
        case ContainerType::Cqm:
            return CqmCodec::load(path, out, nullptr, err);
        case ContainerType::Td0:
            return Td0Codec::load(path, out, nullptr, err);
        case ContainerType::Imd:
            return ImdCodec::load(path, out, nullptr, err);
        case ContainerType::Img:
        default:
            if (!fmt) {
                err = "Rohes Sektorimage (.img) braucht ein Diskettenformat: " + path;
                return false;
            }
            return ImgCodec::load(path, *fmt, out, err);
    }
}

bool ImageCodec::save(const std::string& path, ContainerType type,
                      const DiskFormat* fmt, const DiskMedium& in, std::string& err) {
    switch (type) {
        case ContainerType::Hfe:
            return HfeCodec::save(path, in, err);
        case ContainerType::Dmk:
            return DmkCodec::save(path, in, err);
        case ContainerType::Cqm:
            return CqmCodec::save(path, in, err);
        case ContainerType::Td0:
            return Td0Codec::save(path, in, err);
        case ContainerType::Imd:
            return ImdCodec::save(path, in, err);
        case ContainerType::Img:
        default:
            if (!fmt) {
                err = "Rohes Sektorimage (.img) braucht ein Diskettenformat: " + path;
                return false;
            }
            return ImgCodec::save(path, *fmt, in, err);
    }
}
