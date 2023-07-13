#pragma once

#include <icu.h>

class TextBuffer;

UText* UTextFromTextBuffer(UText* ut, const TextBuffer& textBuffer, UErrorCode* status) noexcept;
til::point_span BufferPositionFromUText(UText* ut, int64_t nativeIndex) noexcept;
