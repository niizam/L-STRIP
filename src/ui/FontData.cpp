// SPDX-License-Identifier: MIT
// Embeds the Inter fonts (SIL OFL 1.1, see assets/fonts/OFL-Inter.txt) into the binary.
#include "FontData.hpp"

#ifndef LSTRIP_FONT_DIR
#error "LSTRIP_FONT_DIR must point at assets/fonts"
#endif

#define LSTRIP_INCBIN(sym, file)                                         \
    __asm__(".section .rodata\n"                                         \
            ".balign 16\n"                                               \
            ".global " #sym "\n" #sym ":\n"                              \
            ".incbin \"" LSTRIP_FONT_DIR "/" file "\"\n"                 \
            ".global " #sym "_end\n" #sym "_end:\n"                      \
            ".byte 0\n"                                                  \
            ".previous\n");                                              \
    extern "C" const unsigned char sym[];                                \
    extern "C" const unsigned char sym##_end[];

LSTRIP_INCBIN(lstrip_inter_regular, "Inter-Regular.ttf")
LSTRIP_INCBIN(lstrip_inter_semibold, "Inter-SemiBold.ttf")

namespace lstrip::ui {
FontBlob interRegular() { return {lstrip_inter_regular, static_cast<unsigned>(lstrip_inter_regular_end - lstrip_inter_regular)}; }
FontBlob interSemiBold() { return {lstrip_inter_semibold, static_cast<unsigned>(lstrip_inter_semibold_end - lstrip_inter_semibold)}; }
} // namespace lstrip::ui
