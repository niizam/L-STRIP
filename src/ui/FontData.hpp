// SPDX-License-Identifier: MIT
#pragma once
namespace lstrip::ui {
struct FontBlob { const unsigned char* data; unsigned size; };
FontBlob interRegular();
FontBlob interSemiBold();
}
